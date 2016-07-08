
#include "hoomd/ExecutionConfiguration.h"

//! Name the unit test module
UP_TEST(moves)
#include "hoomd/test/upp11_config.h"

#include "hoomd/extern/saruprng.h"
#include "hoomd/BoxDim.h"
#include "hoomd/HOOMDMath.h"
#include "hoomd/VectorMath.h"

#include "hoomd/hpmc/Moves.h"
#include "hoomd/hpmc/IntegratorHPMCMono.h"

#include <iostream>

#include <boost/bind.hpp>
#include <hoomd/extern/pybind/include/pybind11/pybind11.h>
#include <boost/function.hpp>
#include <memory>

using namespace std;
using namespace hpmc;
using namespace hpmc::detail;

struct ShapeDummy
    {
        vec3<Scalar> pos;
        quat<Scalar> orientation;
    };

UP_TEST( rand_rotate_3d )
    {
    Saru rng(123, 456, 789);

    quat<Scalar> a(1, vec3<Scalar>(0,0,0));
    for (int i=0; i<10000; i++)
        {
        // move the shape
        quat<Scalar> prev = a;
        move_rotate(a, rng, 1.0, 3);
        quat<Scalar> delta(prev.s - a.s, prev.v - a.v);

        // check that all coordinates moved
        // yes, it is possible that one of the random numbers is zero - if that is the case we can pick a different
        // seed so that we do not sample that case
        UPP_ASSERT(fabs(delta.s) > 0);
        UPP_ASSERT(fabs(delta.v.x) > 0);
        UPP_ASSERT(fabs(delta.v.y) > 0);
        UPP_ASSERT(fabs(delta.v.z) > 0);

        // check that it is a valid rotation
        MY_CHECK_CLOSE(norm2(a), 1, tol);
        }
    }

UP_TEST( rand_rotate_2d )
    {
    Saru rng(123, 456, 789);

    Scalar a = .1;

    quat<Scalar> o(1, vec3<Scalar>(0,0,0));
    for (int i=0; i<10000; i++)
        {
        // move the shape
        quat<Scalar> prev = o;
        move_rotate(o, rng, a, 2);
        quat<Scalar> delta(prev.s - o.s, prev.v - o.v);

        // check that the angle coordinate moved and that the 0 components stayed 0
        // yes, it is possible that one of the random numbers is zero - if that is the case we can pick a different
        // seed so that we do not sample that case
        UPP_ASSERT(fabs(delta.s) > 0);
        MY_CHECK_SMALL(o.v.x, tol_small);
        MY_CHECK_SMALL(o.v.y, tol_small);
        UPP_ASSERT(fabs(delta.v.z) > 0);

        // check that it is a valid rotation
        MY_CHECK_CLOSE(norm2(o), 1, tol);

        // check that the angle of the rotation is not too big
        UPP_ASSERT( (acos(prev.s)*2.0 - acos(o.s)*2.0) <= a);
        }
    }

UP_TEST( rand_translate_3d )
    {
    Saru rng(123, 456, 789);
    Scalar d = 0.1;
    // test randomly generated quaternions for unit norm

    vec3<Scalar> a(0,0,0);
    for (int i=0; i<10000; i++)
        {
        // move the shape
        vec3<Scalar> prev = a;
        move_translate(a, rng, d, 3);
        vec3<Scalar> delta = prev - a;

        // check that all coordinates moved
        // yes, it is possible that one of the random numbers is zero - if that is the case we can pick a different
        // seed so that we do not sample that case
        UPP_ASSERT(fabs(delta.x) > 0);
        UPP_ASSERT(fabs(delta.y) > 0);
        UPP_ASSERT(fabs(delta.z) > 0);

        // check that the move distance is appropriate
        UPP_ASSERT(sqrt(dot(delta,delta)) <= d);
        }
    }

UP_TEST( rand_translate_2d )
    {
    Saru rng(123, 456, 789);
    Scalar d = 0.1;
    // test randomly generated quaternions for unit norm

    vec3<Scalar> a(0,0,0);
    for (int i=0; i<100; i++)
        {
        // move the shape
        vec3<Scalar> prev = a;
        move_translate(a, rng, d, 2);
        vec3<Scalar> delta = prev - a;

        // check that all coordinates moved
        // yes, it is possible that one of the random numbers is zero - if that is the case we can pick a different
        // seed so that we do not sample that case
        UPP_ASSERT(fabs(delta.x) > 0);
        UPP_ASSERT(fabs(delta.y) > 0);
        UPP_ASSERT(delta.z == 0);

        // check that the move distance is appropriate
        UPP_ASSERT(sqrt(dot(delta,delta)) <= d);
        }
    }

void test_rand_select(const unsigned int max)
    {
    Saru rng(123, 456, 789);

    const unsigned int nsamples = (max+1)*1000000;
    unsigned int counts[max+1];

    for (unsigned int i = 0; i <= max; i++)
        counts[i] = 0;

    for (unsigned int i = 0 ; i < nsamples; i++)
        {
        counts[rand_select(rng, max)]++;
        }

    /*for (unsigned int i = 0; i <= max; i++)
        std::cout << double(counts[i])/double(nsamples) << std::endl;*/

    for (unsigned int i = 0; i <= max; i++)
         MY_CHECK_CLOSE(double(counts[i])/double(nsamples), 1.0/double(max+1), 0.5);
    }

UP_TEST( rand_select_test )
    {
    for (unsigned int max=0; max < 10; max++)
        {
        //std::cout << "Testing max=" << max << std::endl;
        test_rand_select(max);
        }

    //std::cout << "Testing max=" << 100 << std::endl;
    test_rand_select(100);
    }

void test_update_order(const unsigned int max)
    {
    // do a simple check on the update order, just make sure that the first index is evenly distributed between 0 and N-1
    const unsigned int nsamples = 1000000;
    unsigned int counts[2];

    for (unsigned int i = 0; i < 2; i++)
        counts[i] = 0;

    UpdateOrder o(10, max);
    for (unsigned int i = 0 ; i < nsamples; i++)
        {
        o.shuffle(i);
        if (o[0] == 0)
            {
            counts[0]++;
            }
        else if (o[0] == max-1)
            {
            counts[1]++;
            }
        else
            {
            cout << "invalid count: " << o[0] << endl;
            UPP_ASSERT(false);
            }
        }

    MY_CHECK_CLOSE(double(counts[0])/double(nsamples), 0.5, 0.5);
    MY_CHECK_CLOSE(double(counts[1])/double(nsamples), 0.5, 0.5);
    }

UP_TEST( update_order_test )
    {
    for (unsigned int max=2; max < 10; max++)
        {
        //std::cout << "Testing max=" << max << std::endl;
        test_update_order(max);
        }
    }


