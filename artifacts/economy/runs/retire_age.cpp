// Each resident's own retiring age, as Society::retireAge (Core/RatwCareers.cpp) gives it: a lot by its id. For the
// stability scorecard (stability.py), which leaves the household's dependents out of the inequality measure (the user,
// 2026-10-30). Reads ids, one a line; writes "id,age". Build with the compiler the game is built with (the same
// std::hash): g++ -O2 -std=c++20 retire_age.cpp -o retire_age
#include <functional>
#include <iostream>
#include <string>
int main()
{
    constexpr int RetireLeast = 58, RetireMost = 66;    // (Core/RatwSociety.h.)
    for (std::string id; std::getline(std::cin, id);)
        std::cout << id << ',' << RetireLeast + int(std::hash<std::string>{}(id + "|retire") % (RetireMost - RetireLeast + 1)) << '\n';
}
