// @file wse_fast_indexing_contract.cpp
// @brief Keep fast indexing available through all public Core data classes.
#include "../support/FastIndexingContract.h"
#include <iostream>
int main()
{
    try
    {
        wse_test::verifyFastIndexing();
        std::cout << "Fast indexing contract passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
