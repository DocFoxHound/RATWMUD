// A stand-in for docker.exe on Windows (Package/testdata/windows-scripts.ps1, the desktop-windows workflow's scripts job):
// logs each call to %FAKE_LOG% and answers the calls restore.bat and reset-editor-password.bat make, as the real backup
// service and ratw-editor would. It reads all of stdin first, as docker compose run and exec do (so a script that gives
// it the console's answers loses them, and the test fails). FAKE_REFUSE=1: the restore is refused.
using System;
using System.IO;
using System.Linq;

class FakeDocker {
    static int Main(string[] args) {
        File.AppendAllText(Environment.GetEnvironmentVariable("FAKE_LOG"), string.Join(" ", args) + "\r\n");
        if (Console.IsInputRedirected) Console.In.ReadToEnd();
        if (args.Length == 0 || args[0] != "compose") return 1;
        string rest = string.Join(" ", args.Skip(1));
        if (rest == "version") { Console.WriteLine("Docker Compose version v5.1.1"); return 0; }
        if (rest == "stop backup" || rest == "stop world" || rest == "up -d") return 0;
        if (rest == "run --rm -T backup list") { Console.WriteLine("Your world's backups, newest first:"); Console.WriteLine("    1  2026-11-12 14:03 UTC  by hand"); return 0; }
        if (rest == "run --rm -T backup name 1") { Console.WriteLine("ratw-20261112T140300Z-by-hand"); return 0; }
        if (rest.StartsWith("run --rm -T backup name ")) { Console.Error.WriteLine("There is no backup numbered that."); return 2; }
        if (rest.StartsWith("run --rm -T backup show ratw-")) { Console.WriteLine("This puts your world back to 2026-11-12 14:03 UTC. Everything players did since then is lost."); return 0; }
        if (rest.StartsWith("run --rm -T backup restore ratw-")) {
            if (Environment.GetEnvironmentVariable("FAKE_REFUSE") == "1") { Console.Error.WriteLine("Your world is running"); return 2; }
            Console.WriteLine("Done"); return 0;
        }
        if (rest == "exec -T world true") return 0;
        if (rest == "exec -T world ratw-editor list") { Console.WriteLine("host-k7q2   owner   on   devices: 1"); return 0; }
        if (rest.StartsWith("exec -T world ratw-editor reset ") && rest.EndsWith(" --write-file")) {
            Console.WriteLine("Editor account: " + args[5]); Console.WriteLine("ratw-editor:abc"); return 0;
        }
        Console.Error.WriteLine("fake docker: unexpected: " + rest);
        return 1;
    }
}
