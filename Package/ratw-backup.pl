#!/usr/bin/perl
# The world's backup service (Docs/Design/78, sections 12.1 and 12.2; Phase M5). It runs on the postgres:18 image (its
# pg_dump matches the server; trixie's own client is 17 and can't dump 18), which has Perl but no Python, so it is Perl
# with core modules only. compose.yaml carries it inline (configs: backup-run); this file is the one to edit, then
#   python3 Package/host_folder.py --sync-compose
# puts it back into compose.yaml (Package/test_package.py fails while they differ).
#
#   serve            the service: waits for the world, then dumps it on start and whenever the newest daily dump is
#                    over 24 hours old, dumps each build's map once, and acts on requests (by hand, before a Publish,
#                    before an update, pins); old dumps pruned; its state in the requests volume for the Editor
#   list             the dumps, newest first, numbered        name N     the dump numbered N
#   show NAME        what restoring NAME means, in one line   now [KIND] a dump now (by hand unless KIND says)
#   restore NAME --keep-accounts | --take-accounts
#                    puts the world back to NAME: refused while the world runs, for another world's dump, a damaged one
#                    or one from a newer package; a safety dump first; today's Editor accounts kept or the backup's taken;
#                    every Editor session ended; the accounts the world now has printed
#
# Files, in the world folder's backups/ (the host's; each made the folder owner's, mode 0600: they hold players' data and
# the Editor accounts' password hashes):
#   ratw-YYYYMMDDTHHMMSSZ-KIND.dump   pg_dump -Fc as ratw_owner, without world.build_cells' rows (most of the database:
#                                     each build's cells are dumped once instead, Q15)
#   ratw-YYYYMMDDTHHMMSSZ-KIND.json   its manifest: world id, package, migration level, the build and every build
#                                     file it needs (with their SHA-256), its own SHA-256 and size, and a pin
#   build-ID.cells.gz, build-ID.json  one build's cells (COPY text, gzipped), and its manifest
# KIND is daily, by-hand, before-publish, before-update or before-restore. Kept: the 7 newest dailies, the 5 newest of
# the rest, none of them past 90 days but the newest backup, and up to 5 the owner pins, each pin for at most 90 days; build files while a kept dump (or the world)
# needs them. A dump that would leave under 2 GB free on the disk is refused (RATW_BACKUP_MIN_FREE_MB).
#
# The requests volume (/requests here; root's in the world's container): kind-only files the world's entrypoint drops
# (backup-by-hand.HEX, backup-before-publish.HEX, before-update.HEX, pin-STAMP.HEX, unpin-STAMP.HEX), each answered in
# .result.HEX; this service's state in .backup-status.json (the entrypoint copies it, checked, to the Editor); the
# world image's package and migration level in .world-image.json (the entrypoint's); .lock while a dump or a restore
# runs; .kept-accounts.json, today's Editor accounts a restore of an older backup keeps, which the entrypoint puts back
# once its migrations have brought that backup's database up to date (it is removed then). Other dot files
# (.ever-official) are left alone.
use strict;
use warnings;
use JSON::PP;
use Digest::SHA;
use Fcntl qw(:flock O_WRONLY O_CREAT O_EXCL);
use POSIX qw(strftime);

my $BACKUPS = $ENV{RATW_BACKUPS_DIR} || '/backups';
my $REQUESTS = $ENV{RATW_BACKUP_REQUESTS} || '/requests';
my $MANAGE = $ENV{RATW_MANAGE_DIR} || '/manage';
my $SECRETS = $ENV{RATW_SECRETS_FILE} || '/run/ratw-secrets/passwords.env';
my $DB = $ENV{RATW_WORLD_DATABASE} || 'ratw_world';
my $DB_HOST = $ENV{RATW_DB_HOST} || 'db';
my $MIN_FREE_MB = ($ENV{RATW_BACKUP_MIN_FREE_MB} // '') =~ /^(\d{1,9})$/ ? $1 : 2048;
my $EVERY = ($ENV{RATW_BACKUP_EVERY} // '') =~ /^(\d{1,9})$/ ? $1 : 86400;
my ($KEEP_DAILY, $KEEP_OTHER, $PINS, $PIN_DAYS) = (7, 5, 5, 90);
my @KINDS = qw(daily by-hand before-publish before-update before-restore);
my %WHY = ('daily' => 'every day', 'by-hand' => 'by hand', 'before-publish' => 'before a map was published',
           'before-update' => 'before an update', 'before-restore' => 'before a restore');
my $KIND = join '|', @KINDS;
my $DUMP = qr/^ratw-(\d{8}T\d{6}Z)-($KIND)$/;
my $STAMP = qr/^\d{8}T\d{6}Z$/;
my $REQUEST = qr/^(backup-by-hand|backup-before-publish|before-update|(?:pin|unpin)-\d{8}T\d{6}Z)\.([0-9a-f]{12})$/;
my $JSON = JSON::PP->new->canonical->utf8;
my %PASSWORD;
my $KEPT = "$REQUESTS/.kept-accounts.json";
my %HOLD;           # Dumps never pruned while this runs: the one a restore is putting back (its safety copy prunes).

sub say_ { print "RATW_BACKUP @_\n"; }
sub fail_ { print STDERR "RATW_BACKUP @_\n"; exit 1; }

# -------------------------------------------------------------------------------------------------------- the database

sub passwords {
    return if %PASSWORD;
    open my $in, '<', $SECRETS or fail_("the database passwords can't be read ($SECRETS: $!): is the db service up?");
    while (my $line = <$in>) {
        chomp $line;
        $PASSWORD{$1} = $2 if $line =~ /^(POSTGRES_PASSWORD|RATW_OWNER_PASSWORD)=(.+)$/;
    }
    fail_("$SECRETS has no owner password") unless $PASSWORD{RATW_OWNER_PASSWORD} && $PASSWORD{POSTGRES_PASSWORD};
}

# A role's connection, as environment for psql, pg_dump and pg_restore (never on a command line).
sub env_for {
    my ($role, $database) = @_;
    passwords();
    my %env = (PGHOST => $DB_HOST, PGPORT => 5432, PGDATABASE => $database // $DB, PGCONNECT_TIMEOUT => 10,
               PGUSER => $role eq 'super' ? 'postgres' : 'ratw_owner',
               PGPASSWORD => $role eq 'super' ? $PASSWORD{POSTGRES_PASSWORD} : $PASSWORD{RATW_OWNER_PASSWORD});
    return %env;
}

# A command with extra environment; returns (exit code, its output). Output is read whole (small here).
sub run_with {
    my ($env, @command) = @_;
    my $pid = open(my $out, '-|') // die "fork: $!";
    if (!$pid) {
        $ENV{$_} = $env->{$_} for keys %$env;
        open STDERR, '>&', \*STDOUT;
        exec { $command[0] } @command or exit 127;
    }
    local $/;
    my $text = <$out> // '';
    close $out;
    return ($? >> 8, $text);
}

# One query (constant text, or numbers checked first), its rows as lines of tab-separated fields; undef on failure.
sub query {
    my ($sql, %o) = @_;
    my ($code, $text) = run_with({env_for($o{role} // 'owner', $o{database})}, 'psql', '-XAtq', '-v', 'ON_ERROR_STOP=1',
                                 '-F', "\t", '-c', $sql);
    if ($code != 0) {
        say_("note: a query failed: " . (split /\n/, $text)[0]) if $o{loud};
        return undef;
    }
    chomp $text;
    return $text;
}

sub database_exists {
    my $found = query("SELECT 1 FROM pg_database WHERE datname = '$DB'", role => 'super', database => 'postgres');
    return defined $found && $found eq '1';
}

# The world is set up: the package is imported (admin.settings' "package", or the entrypoint's "package.ran").
sub world_ready {
    my $found = query("SELECT 1 FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
                       WHERE n.nspname = 'admin' AND c.relname = 'settings'");
    return 0 unless defined $found && $found eq '1';
    my $ready = query("SELECT 1 FROM admin.settings WHERE key IN ('package', 'package.ran') LIMIT 1");
    return defined $ready && $ready eq '1';
}

# What a dump records of the world it came from: its id (world.toml's, as the entrypoint recorded it), its package,
# its migration level and its builds.
sub world_facts {
    my %o = @_;
    my %facts = (world => '', package => '', migration => '', builds => []);
    my $ran = query("SELECT value::text FROM admin.settings WHERE key = 'package.ran'", %o);
    my $made = query("SELECT value::text FROM admin.settings WHERE key = 'package'", %o);
    my $r = eval { $ran ? $JSON->decode($ran) : {} } || {};
    my $m = eval { $made ? $JSON->decode($made) : {} } || {};
    $facts{world} = clean_world($r->{world});
    $facts{package} = clean_text($r->{version} // $m->{version} // '');
    my $level = query('SELECT max(version) FROM public.schema_migrations', %o) // '';
    $facts{migration} = $level =~ /^(\d{1,6})$/ ? $1 : '';
    my $builds = query('SELECT id FROM world.builds ORDER BY id', %o) // '';
    $facts{builds} = [grep { /^\d{1,18}$/ } split /\n/, $builds];
    if (!$facts{world}) {
        my $image = read_json("$REQUESTS/.world-image.json");
        $facts{world} = clean_world($image->{world});
    }
    return \%facts;
}

sub clean_world { my ($w) = @_; return defined $w && $w =~ /^((?:world|local)-[0-9a-f]{32})$/ ? $1 : ''; }
sub clean_text { my ($t) = @_; $t = '' unless defined $t; $t =~ s/[^A-Za-z0-9._+-]//g; return substr($t, 0, 40); }

# ------------------------------------------------------------------------------------------------------------ files

sub read_json {
    my ($path) = @_;
    return {} if -l $path || !-f $path || -s $path > 1024 * 1024;
    open my $in, '<:raw', $path or return {};
    local $/;
    my $value = eval { $JSON->decode(<$in> // '') };
    return ref $value eq 'HASH' ? $value : {};
}

# The backups folder's owner, which every file made there gets (so the host can open, move and delete them).
sub folder_owner {
    my @s = stat $BACKUPS;
    return @s ? ($s[4], $s[5]) : (0, 0);
}

# A file written whole: a new one beside it, made the folder owner's with `mode`, then renamed into place.
sub write_whole {
    my ($path, $body, $mode, $owner) = @_;
    my $temp = $path =~ s{([^/]+)$}{.$1.part}r;
    unlink $temp;
    sysopen(my $out, $temp, O_WRONLY | O_CREAT | O_EXCL, $mode // 0600) or die "can't write $temp: $!";
    binmode $out;
    print {$out} $body or die "can't write $temp: $!";
    close $out or die "can't write $temp: $!";
    my @who = $owner ? @$owner : folder_owner();
    chown @who, $temp;
    chmod $mode // 0600, $temp;
    rename $temp, $path or die "can't rename $temp: $!";
}

sub sha256_of {
    my ($path) = @_;
    my $sha = Digest::SHA->new(256);
    $sha->addfile($path, 'b');
    return $sha->hexdigest;
}

# Free bytes on the backups folder's disk.
sub free_bytes {
    my ($code, $text) = run_with({}, 'df', '-Pk', $BACKUPS);
    return undef if $code != 0;
    my @lines = split /\n/, $text;
    my @f = split ' ', ($lines[1] // '');
    return defined $f[3] && $f[3] =~ /^\d+$/ ? $f[3] * 1024 : undef;
}

# Whether a file of about `need` bytes fits and leaves the minimum free: (1) or (0, free bytes).
sub room_for {
    my ($need) = @_;
    my $free = free_bytes();
    return (1, $free) unless defined $free;
    return ($free - $need >= $MIN_FREE_MB * 1024 * 1024 ? 1 : 0, $free);
}

sub mb { my ($bytes) = @_; return sprintf('%.1f', ($bytes // 0) / (1024 * 1024)); }

sub manifests {
    opendir(my $dir, $BACKUPS) or return ();
    my @found;
    for my $file (readdir $dir) {
        next unless $file =~ /^(ratw-\d{8}T\d{6}Z-(?:$KIND))\.json$/;
        my $name = $1;
        next unless -f "$BACKUPS/$name.dump" && !-l "$BACKUPS/$name.dump";
        my $m = read_json("$BACKUPS/$file");
        next unless ($m->{name} // '') eq $name;
        push @found, $m;
    }
    closedir $dir;
    return sort { $b->{name} cmp $a->{name} } @found;     # (Newest first: the stamp sorts.)
}

sub pinned { my ($m, $now) = @_; return ($m->{pinnedUntil} // 0) > ($now // time) ? 1 : 0; }

# ------------------------------------------------------------------------------------------------------------ state

my %STATE = (problem => undef, last => undef, working => '');

sub ago {
    my ($seconds) = @_;
    return 'just now' if $seconds < 90;
    return sprintf('%d minutes ago', $seconds / 60 + 0.5) if $seconds < 90 * 60;
    return sprintf('%d hours ago', $seconds / 3600 + 0.5) if $seconds < 36 * 3600;
    return sprintf('%d days ago', $seconds / 86400 + 0.5);
}

sub when_text { my ($at) = @_; return strftime('%Y-%m-%d %H:%M UTC', gmtime $at); }

# .backup-status.json: what the Editor's My world shows (the entrypoint checks it and copies it to the admin volume).
sub write_status {
    my $now = time;
    my @all = manifests();
    my $used = 0;
    if (opendir(my $dir, $BACKUPS)) {
        for my $file (readdir $dir) {
            next unless $file =~ /^(?:ratw-\d{8}T\d{6}Z-[a-z-]+\.(?:dump|json)|build-\d+\.(?:cells\.gz|json))$/;
            $used += -s "$BACKUPS/$file" // 0;
        }
        closedir $dir;
    }
    my $newest = $all[0];
    my %status = (v => 1, at => $now, working => $STATE{working} || undef, problem => $STATE{problem},
                  disk => {free => free_bytes(), used => $used, minFree => $MIN_FREE_MB * 1024 * 1024},
                  last => $newest ? {name => $newest->{name}, kind => $newest->{kind}, at => $newest->{at},
                                     size => $newest->{size}} : undef,
                  pins => scalar(grep { pinned($_, $now) } @all), pinCap => $PINS,
                  backups => [map { {name => $_->{name}, kind => $_->{kind}, at => $_->{at}, size => $_->{size},
                                     package => $_->{package}, migration => $_->{migration},
                                     pinnedUntil => pinned($_, $now) ? $_->{pinnedUntil} : undef} } @all[0 .. ($#all < 59 ? $#all : 59)]]);
    eval { write_whole("$REQUESTS/.backup-status.json", $JSON->encode(\%status) . "\n", 0600, [0, 0]); 1 }
        or say_("warning: the backups' state couldn't be written ($@)");
}

sub problem {
    my ($code, %more) = @_;
    $STATE{problem} = {code => $code, at => time, %more};
}

sub problem_code { return $STATE{problem} ? $STATE{problem}{code} : ''; }

# A refusal for a full disk in the last 10 minutes: the service's own dumps wait that long before trying again (a request
# is always tried).
sub disk_full { return problem_code() eq 'disk' && time - $STATE{problem}{at} < 600; }

# ------------------------------------------------------------------------------------------------------------- dumps

# The size a new dump will about be: the newest one's, or the database without the build cells.
sub dump_estimate {
    my @all = manifests();
    return $all[0]{size} if @all && ($all[0]{size} // 0) > 0;
    my $size = query("SELECT pg_database_size(current_database())
                             - coalesce(pg_total_relation_size(to_regclass('world.build_cells')), 0)") // 0;
    return $size =~ /^\d+$/ ? $size : 0;
}

# Every build's cells, once, to build-ID.cells.gz (Q15): each build the world holds that has no file yet.
sub build_files {
    my ($facts) = @_;
    my @made;
    for my $id (@{$facts->{builds}}) {
        next if -f "$BACKUPS/build-$id.cells.gz" && -f "$BACKUPS/build-$id.json";
        my $need = query("SELECT pg_total_relation_size(to_regclass('world.build_cells'))
                                 / greatest((SELECT count(*) FROM world.builds), 1)") // 0;
        $need = $need =~ /^\d+$/ ? int($need / 3) : 0;      # (Gzipped: about a third.)
        my ($fits, $free) = room_for($need);
        if (!$fits) {
            problem('disk', free => $free, minFree => $MIN_FREE_MB * 1024 * 1024);
            say_("refused: build ${id}'s map (about " . mb($need) . " MB) wasn't saved: the backups' disk has " . mb($free)
                 . " MB free, and at least $MIN_FREE_MB MB must stay free. Free some space, or delete old files from backups/");
            return 0;
        }
        my $part = "$BACKUPS/.build-$id.cells.gz.part";
        unlink $part;
        my %env = env_for('owner');
        my ($code, $text) = run_with(\%env, 'bash', '-c',
            'set -o pipefail; umask 077; psql -XAtq -v ON_ERROR_STOP=1 -c "$1" | gzip -c > "$2"', '_',
            "COPY (SELECT build_id, cell_id, header, body, seams FROM world.build_cells WHERE build_id = $id ORDER BY cell_id) TO STDOUT",
            $part);
        if ($code != 0) {
            unlink $part;
            problem('failed');
            say_("warning: build ${id}'s map couldn't be saved: " . (split /\n/, $text)[0]);
            return 0;
        }
        my $rows = query("SELECT count(*) FROM world.build_cells WHERE build_id = $id") // 0;
        chown folder_owner(), $part;
        chmod 0600, $part;
        rename $part, "$BACKUPS/build-$id.cells.gz" or die "rename: $!";
        write_whole("$BACKUPS/build-$id.json", $JSON->encode({v => 1, build => $id + 0, world => $facts->{world},
            rows => $rows + 0, size => -s "$BACKUPS/build-$id.cells.gz", sha256 => sha256_of("$BACKUPS/build-$id.cells.gz"),
            at => time}) . "\n");
        say_("saved build ${id}'s map once (build-$id.cells.gz, " . mb(-s "$BACKUPS/build-$id.cells.gz") . ' MB)');
        push @made, $id;
    }
    return 1;
}

# A dump of the world now: its name, or undef (said why, and in the state).
sub dump_now {
    my ($kind) = @_;
    my $facts = world_facts();
    build_files($facts) or return undef;
    my $need = dump_estimate();
    my ($fits, $free) = room_for($need);
    if (!$fits) {
        problem('disk', free => $free, minFree => $MIN_FREE_MB * 1024 * 1024, kind => $kind);
        say_("refused: a backup ($WHY{$kind}, about " . mb($need) . " MB) wasn't made: the backups' disk has " . mb($free)
             . " MB free, and at least $MIN_FREE_MB MB must stay free. Free some space, or delete old files from backups/");
        return undef;
    }
    my ($stamp, $name, $at);
    for (1 .. 5) {
        $at = time;
        $stamp = strftime('%Y%m%dT%H%M%SZ', gmtime $at);
        $name = "ratw-$stamp-$kind";
        last unless -e "$BACKUPS/$name.dump" || grep { $_->{name} =~ /^ratw-\Q$stamp\E-/ } manifests();
        sleep 1;
    }
    my $part = "$BACKUPS/.$name.dump.part";
    unlink $part;
    $STATE{working} = $kind;
    my ($code, $text) = run_with({env_for('owner'), PGOPTIONS => ''}, 'bash', '-c', 'umask 077; exec pg_dump "$@"', '_',
                                 '-Fc', '--exclude-table-data=world.build_cells', '-f', $part);
    $STATE{working} = '';
    if ($code != 0) {
        unlink $part;
        problem('failed', kind => $kind);
        say_("warning: a backup ($WHY{$kind}) failed: " . join(' ', (split /\n/, $text)[0 .. 1]));
        return undef;
    }
    chown folder_owner(), $part;
    chmod 0600, $part;
    my @builds = map { {id => $_ + 0, file => "build-$_.cells.gz", sha256 => read_json("$BACKUPS/build-$_.json")->{sha256}} }
        @{$facts->{builds}};
    my %manifest = (v => 1, name => $name, kind => $kind, at => $at, world => $facts->{world},
                    package => $facts->{package}, migration => $facts->{migration},
                    build => @builds ? $builds[-1]{id} : undef, builds => \@builds, size => -s $part,
                    sha256 => sha256_of($part), pinnedUntil => undef);
    rename $part, "$BACKUPS/$name.dump" or die "rename: $!";
    write_whole("$BACKUPS/$name.json", $JSON->encode(\%manifest) . "\n");
    $STATE{problem} = undef;
    say_("made a backup ($WHY{$kind}): $name (" . mb($manifest{size}) . ' MB)');
    prune();
    return $name;
}

# Old dumps go: the 7 newest dailies, the 5 newest of the rest and every pinned one are kept; build files stay while a
# kept dump or the world needs them. Only this service's own names are ever removed.
sub prune {
    my $now = time;
    my @all = manifests();
    my (%keep, $daily, $other);
    for my $m (@all) {
        my $is_daily = $m->{kind} eq 'daily';
        # (No unpinned backup outlives $PIN_DAYS, whatever its count, but the newest: a deleted wolf isn't kept in a
        # backup for ever on a world that is seldom updated or seldom on. M6.)
        my $young = $now - ($m->{at} // 0) <= $PIN_DAYS * 86400 || $m->{name} eq $all[0]{name};
        if (pinned($m, $now) || ($young && ($is_daily ? ++$daily <= $KEEP_DAILY : ++$other <= $KEEP_OTHER))) {
            $keep{$m->{name}} = 1;
        }
        $keep{$m->{name}} = 1 if $HOLD{$m->{name}};
    }
    for my $m (@all) {
        next if $keep{$m->{name}};
        unlink "$BACKUPS/$m->{name}.dump", "$BACKUPS/$m->{name}.json";
        say_("removed an old backup: $m->{name}");
    }
    my %needed = map { $_ => 1 } @{world_facts()->{builds}};
    for my $m (@all) {
        next unless $keep{$m->{name}};
        $needed{$_->{id}} = 1 for @{$m->{builds} || []};
    }
    if (opendir(my $dir, $BACKUPS)) {
        for my $file (readdir $dir) {
            if ($file =~ /^build-(\d+)\.(?:cells\.gz|json)$/ && !$needed{$1}) {
                unlink "$BACKUPS/$file";
                say_("removed a map no kept backup needs: $file") if $file =~ /json$/;
            }
            unlink "$BACKUPS/$file" if $file =~ /^\..*\.part$/ && -M "$BACKUPS/$file" > 1;
        }
        closedir $dir;
    }
}

sub set_pin {
    my ($stamp, $on) = @_;
    my $now = time;
    my @all = manifests();
    my ($m) = grep { $_->{name} =~ /^ratw-\Q$stamp\E-/ } @all;
    return {ok => JSON::PP::false, code => 'no-backup'} unless $m;
    if ($on && !pinned($m, $now) && (grep { pinned($_, $now) } @all) >= $PINS) {
        problem('pins');
        say_("refused: $PINS backups are pinned already: unpin one first");
        return {ok => JSON::PP::false, code => 'pins'};
    }
    $m->{pinnedUntil} = $on ? $now + $PIN_DAYS * 86400 : undef;
    write_whole("$BACKUPS/$m->{name}.json", $JSON->encode($m) . "\n");
    say_(($on ? 'pinned' : 'unpinned') . " $m->{name}" . ($on ? " (kept for $PIN_DAYS days)" : ''));
    $STATE{problem} = undef if problem_code() eq 'pins';
    prune() unless $on;
    return {ok => JSON::PP::true, name => $m->{name}};
}

# ---------------------------------------------------------------------------------------------------------- requests

sub lock_or {
    my ($wait) = @_;
    open my $lock, '>', "$REQUESTS/.lock" or return undef;
    return flock($lock, $wait ? LOCK_EX : LOCK_EX | LOCK_NB) ? $lock : undef;
}

sub take_requests {
    opendir(my $dir, $REQUESTS) or return;
    my @names = sort grep { !/^\./ } readdir $dir;
    closedir $dir;
    for my $file (@names) {
        my $path = "$REQUESTS/$file";
        my ($kind, $id) = $file =~ $REQUEST;
        my $plain = -f $path && !-l $path;
        unlink $path;
        next unless $kind && $plain;
        my $result;
        if ($kind =~ /^(pin|unpin)-(\d{8}T\d{6}Z)$/) {
            $result = set_pin($2, $1 eq 'pin');
        } else {
            my $made = dump_now($kind =~ s/^backup-//r);
            $result = $made ? {ok => JSON::PP::true, name => $made}
                            : {ok => JSON::PP::false, code => (problem_code() || 'failed')};
        }
        eval { write_whole("$REQUESTS/.result.$id", $JSON->encode($result) . "\n", 0600, [0, 0]); 1 };
        write_status();
    }
    if (opendir(my $dir, $REQUESTS)) {       # (Answers nobody read within the hour.)
        for my $file (readdir $dir) {
            unlink "$REQUESTS/$file" if $file =~ /^\.result\.[0-9a-f]{12}$/ && -M "$REQUESTS/$file" > 1 / 24;
        }
        closedir $dir;
    }
}

sub daily_due {
    my ($newest) = grep { $_->{kind} eq 'daily' } manifests();
    return !$newest || time - ($newest->{at} // 0) >= $EVERY;
}

sub serve {
    -d $BACKUPS or fail_("no backups folder ($BACKUPS): the world folder's backups/ is mounted there by compose.yaml");
    say_("the backup service: a backup on start and whenever the newest daily one is over " . ($EVERY / 3600)
         . " hours old, each map once, and on the Editor's requests; into backups/ (at least $MIN_FREE_MB MB kept free)");
    my ($said, $last_status, $last_builds, $waiting) = (0, 0, 0, time);
    $SIG{TERM} = $SIG{INT} = sub { say_('stopping'); exit 0; };
    while (1) {
        if (time - $last_status >= 30) {
            write_status();
            $last_status = time;
        }
        if (!world_ready()) {
            if (!$said && time - $waiting > 60) {
                say_('waiting for the world to be set up (its first start imports it)');
                $said = 1;
            }
            sleep 5;
            next;
        }
        if (my $lock = lock_or(0)) {
            take_requests();
            if (time - $last_builds >= 60 && !disk_full()) {
                build_files(world_facts());
                $last_builds = time;
            }
            dump_now('daily') if daily_due() && !disk_full();
            close $lock;
        }
        sleep 5;
    }
}

# ------------------------------------------------------------------------------------------------------------- restore

sub numbered {
    my @all = manifests();
    my ($n) = @_;
    return $n =~ /^\d{1,4}$/ && $n >= 1 && $n <= @all ? $all[$n - 1] : undef;
}

sub line_for {
    my ($m) = @_;
    my $now = time;
    return sprintf('%-22s %-17s %-28s %8s MB  package %s%s', when_text($m->{at}), '(' . ago($now - $m->{at}) . ')',
                   $WHY{$m->{kind}} // $m->{kind}, mb($m->{size}), $m->{package} || '?',
                   pinned($m, $now) ? '  pinned' : '');
}

sub list {
    my @all = manifests();
    if (!@all) {
        print "There are no backups in backups/ yet. The world makes one when it starts, and every day.\n";
        return 1;
    }
    print "Your world's backups, newest first:\n\n";
    my $n = 0;
    printf "  %3d  %s\n", ++$n, line_for($_) for @all;
    print "\n";
    return 0;
}

sub by_name {
    my ($name) = @_;
    return undef unless defined $name && $name =~ $DUMP;
    my ($m) = grep { $_->{name} eq $name } manifests();
    return $m;
}

sub refuse { my ($text) = @_; print STDERR "\n$text\n\nNothing was changed.\n"; exit 2; }

# The accounts of today's world (when it has any), as JSON per table, to put back after the restore.
sub accounts_now {
    my %kept;
    for my $table (qw(admins devices invites locks)) {
        my $exists = query("SELECT to_regclass('dm.$table') IS NOT NULL", role => 'super');
        next unless ($exists // '') eq 't';
        my $rows = query("SELECT coalesce(json_agg(t), '[]'::json) FROM dm.$table t", role => 'super');
        return undef unless defined $rows;
        $kept{$table} = $rows;
    }
    return $kept{admins} ? \%kept : undef;
}

# Today's accounts put back over the backup's: every account row, its devices, invites and locks; columns the restored
# world's tables have, taken from the saved rows (a column it lacks is left out; one it has that they lack keeps its
# default). One transaction, as the superuser.
sub put_accounts {
    my ($kept) = @_;
    my $escape = sub { my $t = shift; $t =~ s/\\/\\\\/g; $t =~ s/\t/\\t/g; $t =~ s/\n/\\n/g; $t =~ s/\r/\\r/g; return $t; };
    my $script = "\\set ON_ERROR_STOP on\nBEGIN;\nCREATE TEMP TABLE kept (name text, rows json);\n"
        . "COPY kept (name, rows) FROM STDIN;\n"
        . join('', map { "$_\t" . $escape->($kept->{$_}) . "\n" } sort keys %$kept) . "\\.\n" . <<'SQL';
DO $$
DECLARE t text; cols text; data json;
BEGIN
    IF to_regclass('dm.admins') IS NULL THEN RETURN; END IF;
    DELETE FROM dm.admins;      -- (Its sessions, devices, invites and locks go with it: ON DELETE CASCADE.)
    FOREACH t IN ARRAY ARRAY['admins', 'devices', 'invites', 'locks'] LOOP
        SELECT rows INTO data FROM kept WHERE name = t;
        IF data IS NULL OR to_regclass('dm.' || t) IS NULL OR json_array_length(data) = 0 THEN CONTINUE; END IF;
        SELECT string_agg(quote_ident(c.column_name), ', ' ORDER BY c.ordinal_position) INTO cols
          FROM information_schema.columns c
         WHERE c.table_schema = 'dm' AND c.table_name = t
           AND c.column_name IN (SELECT json_object_keys(data -> 0));
        EXECUTE format('INSERT INTO dm.%I (%s) SELECT %s FROM json_populate_recordset(NULL::dm.%I, $1)', t, cols, cols, t)
          USING data;
    END LOOP;
END $$;
COMMIT;
SQL
    my %env = env_for('super');
    my $pid = open(my $to, '|-') // die "fork: $!";
    if (!$pid) {
        $ENV{$_} = $env{$_} for keys %env;
        open STDOUT, '>', '/dev/null';
        exec 'psql', '-XAq', '-f', '-' or exit 127;
    }
    print {$to} $script;
    close $to;
    return $? == 0;
}

sub accounts_list {
    my $rows = query("SELECT username, role, disabled FROM dm.admins ORDER BY CASE role WHEN 'owner' THEN 0 WHEN 'builder' THEN 1
                      WHEN 'dm' THEN 2 WHEN 'viewer' THEN 3 ELSE 4 END, username", role => 'super') // '';
    return map { my ($u, $r, $d) = split /\t/; {username => $u, role => $r, off => $d eq 't'} } grep { length } split /\n/, $rows;
}

sub first_editor_after {
    my ($when, @accounts) = @_;
    return unless -d $MANAGE;
    my $list = @accounts ? join('', map { "    $_->{username} ($_->{role}" . ($_->{off} ? ', turned off' : '') . ")\n" } @accounts)
                         : "    none: the world makes a new owner when it starts, and writes it here.\n";
    my $text = "Your world was put back to its backup of $when, with the Editor accounts that backup had:\n\n$list\n"
        . "A one-time password this file held before no longer works. Lost your password? Double-click\n"
        . "reset-editor-password in this folder (reset-editor-password.bat on Windows, reset-editor-password.sh\n"
        . "elsewhere), or type:\n\n    docker compose exec world ratw-editor reset NAME\n";
    my @s = stat $MANAGE;
    eval { write_whole("$MANAGE/first-editor.txt", $text, 0600, [$s[4], $s[5]]); 1 }
        or say_("note: manage/first-editor.txt couldn't be written ($@)");
}

sub restore {
    my ($name, $how) = @_;
    defined $how && $how =~ /^--(keep|take)-accounts$/ or fail_('restore NAME --keep-accounts | --take-accounts');
    my $keep = $1 eq 'keep';
    my $m = by_name($name) or refuse("There is no backup called " . ($name // '') . " in backups/.");
    my $lock = lock_or(0) or refuse('A backup is being made just now: try again in a minute.');
    $HOLD{$m->{name}} = 1;          # (Its safety copy's pruning never takes the backup being put back.)
    print "Checking the backup...\n";
    refuse("This backup is damaged (its file doesn't match its manifest): choose another.")
        unless ($m->{sha256} // '') eq sha256_of("$BACKUPS/$m->{name}.dump");
    my $exists = database_exists();
    if ($exists) {
        my $live = query("SELECT count(*) FROM pg_stat_activity WHERE datname = '$DB' AND usename IN ('ratw_game', 'ratw_dm')",
                         role => 'super', database => 'postgres') // '0';
        refuse("Your world is running: stop it first, in the world folder:\n\n    docker compose stop world\n\n"
               . "(restore.bat and restore.sh do this for you.)") if $live ne '0';
    }
    my $ready = $exists && world_ready();
    my $now = $ready ? world_facts() : {world => '', migration => '', package => ''};
    my $image = read_json("$REQUESTS/.world-image.json");
    my $here = $now->{world} || clean_world($image->{world});
    if ($m->{world} && $here && $m->{world} ne $here) {
        refuse("This backup is from another world ($m->{world}); this one is $here. A world is only put back to its own "
               . "backups.");
    }
    my $level = ($image->{migrations} // '') =~ /^(\d{1,6})$/ ? $1 : $now->{migration};
    my $older = ($m->{migration} // '') =~ /^\d+$/ && ($level // '') =~ /^\d+$/ && $m->{migration} < $level;
    if (($m->{migration} // '') =~ /^\d+$/ && ($level // '') =~ /^\d+$/ && $m->{migration} > $level) {
        refuse("This backup was made by a newer package of the world (its database is at migration $m->{migration}; "
               . "this package's is at $level). Update the world first (keep world.toml, .env and backups), then restore.");
    }
    for my $b (@{$m->{builds} || []}) {
        my $file = "$BACKUPS/build-$b->{id}.cells.gz";
        refuse("This backup needs its map, backups/build-$b->{id}.cells.gz, which is missing or damaged: choose another.")
            unless -f $file && ($b->{sha256} // '') eq sha256_of($file);
    }
    my $kept;
    if ($ready) {
        print "Making a safety copy of your world as it is now...\n";
        my $safety = dump_now('before-restore');
        refuse("No safety copy could be made" . (problem_code() eq 'disk'
               ? ': the disk that holds backups/ is nearly full. Free some space, then try again.' : '.')) unless $safety;
        print "  safety copy: $safety\n";
        if ($keep) {
            $kept = accounts_now();
            refuse("Today's Editor accounts couldn't be read.")
                if !$kept && (query("SELECT to_regclass('dm.admins') IS NOT NULL", role => 'super') // '') eq 't';
        }
    }
    unlink $KEPT;                   # (A restore before this one that never reached a start: its accounts are stale.)
    print "Putting the world back to " . when_text($m->{at}) . "...\n";
    say_("restoring $m->{name} (" . ($keep ? 'keeping' : 'taking the backup\'s') . ' Editor accounts)');
    query("DROP DATABASE IF EXISTS $DB WITH (FORCE)", role => 'super', database => 'postgres', loud => 1) // fail_(
        'the old database could not be removed; nothing else was changed (the safety copy is in backups/)');
    my ($code, $text) = run_with({env_for('super', 'postgres')}, 'pg_restore', '--create', '--exit-on-error', '-d', 'postgres',
                                 "$BACKUPS/$m->{name}.dump");
    if ($code != 0) {
        print STDERR $text;
        fail_("the restore failed part way. Your world as it was is in backups/ (the safety copy): run restore again "
              . "and choose it.");
    }
    for my $b (@{$m->{builds} || []}) {
        my ($c, $t) = run_with({env_for('super')}, 'bash', '-c',
            'set -o pipefail; gzip -dc "$1" | psql -XAq -v ON_ERROR_STOP=1 -c "COPY world.build_cells (build_id, cell_id, header, body, seams) FROM STDIN"',
            '_', "$BACKUPS/build-$b->{id}.cells.gz");
        fail_("build $b->{id}\'s map couldn't be put back: $t (the safety copy is in backups/)") if $c != 0;
    }
    my $took = !$keep;
    my $later;
    if ($keep && $kept && $older) {
        # An older backup's tables lack today's columns (and roles: 0063's owner and builder): its accounts go back in
        # when the world starts, after its migrations (Package/entrypoint.py, put_back_accounts).
        my $body = '{"v":1,"from":' . $JSON->encode($m->{name}) . ',"tables":{'
            . join(',', map { $JSON->encode($_) . ':' . $kept->{$_} } sort keys %$kept) . "}}\n";
        eval { write_whole($KEPT, $body, 0600, [0, 0]); 1 }
            or fail_("today's Editor accounts couldn't be kept for the world's start ($@; the safety copy is in backups/; "
                     . 'the world has the backup\'s accounts now)');
        $later = eval { $JSON->decode($kept->{admins}) } || [];
    } elsif ($keep && $kept) {
        put_accounts($kept) or fail_("today's Editor accounts couldn't be put back over the backup's (the safety copy is "
                                     . 'in backups/; the world has the backup\'s accounts now)');
    } elsif ($keep) {
        print "  Your world had no Editor accounts to keep: it has the backup's.\n";
        $took = 1;
    }
    query("DO \$x\$ BEGIN IF to_regclass('dm.sessions') IS NOT NULL THEN DELETE FROM dm.sessions; END IF; END \$x\$",
          role => 'super', loud => 1);
    my @accounts = $later ? (sort { $a->{username} cmp $b->{username} }
                             map { {username => $_->{username}, role => $_->{role}, off => $_->{disabled} ? 1 : 0} } @$later)
                 : (query("SELECT to_regclass('dm.admins') IS NOT NULL", role => 'super') // '') eq 't' ? accounts_list() : ();
    first_editor_after(when_text($m->{at}), @accounts) if $took;
    print "\nThis backup comes from an older package: your world brings it up to date when it starts, and then puts\n"
        . "your Editor accounts back as they were.\n" if $later;
    print "\nDone: your world is back to " . when_text($m->{at}) . ". Every Editor session has ended.\n";
    print "Its Editor accounts" . ($took ? ' (the backup\'s)' : ' (kept as they were)') . ":\n";
    print @accounts ? join('', map { "    $_->{username} ($_->{role}" . ($_->{off} ? ', turned off' : '') . ")\n" } @accounts)
                    : "    none yet: the world makes its owner when it starts, in manage/first-editor.txt\n";
    say_("restored $m->{name}");
    close $lock;
    return 0;
}

# ---------------------------------------------------------------------------------------------------------------- main

# (Only when run, not when a test loads it with `do` to call one part.)
return 1 if caller;
my $command = shift @ARGV // 'serve';
if ($command eq 'serve') {
    serve();
} elsif ($command eq 'list') {
    exit list();
} elsif ($command eq 'name') {
    my $m = numbered($ARGV[0] // '') or do { print STDERR "There is no backup numbered " . ($ARGV[0] // '') . ".\n"; exit 2 };
    print "$m->{name}\n";
} elsif ($command eq 'show') {
    my $m = by_name($ARGV[0]) or do { print STDERR "There is no backup called " . ($ARGV[0] // '') . " in backups/.\n"; exit 2 };
    print "\nThis puts your world back to " . when_text($m->{at}) . ' (' . ago(time - $m->{at}) . ', '
          . ($WHY{$m->{kind}} // $m->{kind}) . "). Everything players did since then is lost.\n\n";
} elsif ($command eq 'now') {
    my $kind = $ARGV[0] // 'by-hand';
    grep { $_ eq $kind } @KINDS or fail_("now [" . join('|', @KINDS) . ']');
    world_ready() or fail_('the world isn\'t set up yet');
    my $lock = lock_or(1);
    my $made = dump_now($kind);
    write_status();
    exit($made ? 0 : 1);
} elsif ($command eq 'restore') {
    exit restore(@ARGV);
} else {
    fail_("unknown command $command (serve, list, name N, show NAME, now [KIND], restore NAME --keep-accounts|--take-accounts)");
}
