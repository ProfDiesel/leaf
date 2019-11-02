// Copyright (c) 2018-2019 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#ifdef LEAF_NO_EXCEPTIONS

#include <iostream>

int main()
{
	std::cout << "Unit test not applicable." << std::endl;
	return 0;
}

#else

#include <boost/leaf/preload.hpp>
#include <boost/leaf/handle_exception.hpp>
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct info { int value; };

void g1()
{
	auto load = leaf::accumulate( []( info & x ) { ++x.value; } );
}

void g2()
{
	throw std::exception();
}

void f()
{
	auto load = leaf::preload( info{42} );
	g1();
	g2();
}

int main()
{
	int r = leaf::try_catch(
		[]
		{
			f();
			return 0;
		},
		[]( info x )
		{
			BOOST_TEST_EQ(x.value, 42);
			return 1;
		},
		[]
		{
			return 2;
		 } );
	BOOST_TEST_EQ(r, 1);

	return boost::report_errors();
}

#endif
