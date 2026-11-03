# PROTOTYPE (what-if only): RATW_P_BANK=1: a crew's hours in hand are kept up to its longest craft (plus an hour), not
# two hours' worth, so a lone cartographer can make a 13.9-hour map over two days.
import sys
p = sys.argv[1]
s = open(p).read()
old = """    bank = std::min(2. * crew, bank + crew);"""
assert s.count(old) == 1
s = s.replace(old, """    double most = 2. * crew;
    if (static const bool longer = knob("RATW_P_BANK", 0) > 0; longer && business)
        for (const auto* k : items::craftsFor(business->id))
            most = std::max(most, k->hours + crew);
    bank = std::min(most, bank + crew);""")
open(p, 'w').write(s)
print('ok')
