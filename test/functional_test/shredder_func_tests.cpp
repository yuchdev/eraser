#include <eraser/shredder_file_properties.h>

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>

using namespace shredder;
using namespace boost::unit_test;

// Functional tests
/*
*/

#pragma region GeneralShredderFunctionalTests

///////////////////////////////////
// Test cases

BOOST_AUTO_TEST_SUITE(GeneralShredderFunctionalTests);


BOOST_AUTO_TEST_CASE(TestShredderFileProperties)
{
    ShredderFileProperties p;
    p.set_system_added(true);
    BOOST_CHECK_EQUAL(p.is_system_added(), true);
    BOOST_CHECK_EQUAL(p.is_file(), false);

    p.set_is_file(true);
    BOOST_CHECK_EQUAL(p.is_system_added(), true);
    BOOST_CHECK_EQUAL(p.is_file(), true);

    p.set_system_added(false);
    BOOST_CHECK_EQUAL(p.is_system_added(), false);
    BOOST_CHECK_EQUAL(p.is_file(), true);

    p.set_is_file(false);
    BOOST_CHECK_EQUAL(p.is_system_added(), false);
    BOOST_CHECK_EQUAL(p.is_file(), false);

    // All files are false, value should be 0
    BOOST_CHECK_EQUAL(p.get_flags(), 0LL);
}

#pragma endregion

BOOST_AUTO_TEST_SUITE_END()
