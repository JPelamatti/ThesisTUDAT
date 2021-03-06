/*    Copyright (c) 2010-2015, Delft University of Technology
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without modification, are
 *    permitted provided that the following conditions are met:
 *      - Redistributions of source code must retain the above copyright notice, this list of
 *        conditions and the following disclaimer.
 *      - Redistributions in binary form must reproduce the above copyright notice, this list of
 *        conditions and the following disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *      - Neither the name of the Delft University of Technology nor the names of its contributors
 *        may be used to endorse or promote products derived from this software without specific
 *        prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 *    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 *    OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *    Changelog
 *      YYMMDD    Author            Comment
 *      130226    D. Dirkx          Migrated from personal code.
 *      130521    E. Brandon        Minor changes during code check.
 *      140127    D. Dirkx          Adapted for custom Spice kernel folder.
 *
 *    References
 *
 *    Notes
 *
 */

#define BOOST_TEST_MAIN

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>

#include "Tudat/Astrodynamics/ObservationModels/lightTimeSolution.h"
#include "Tudat/Astrodynamics/ObservationModels/UnitTests/testLightTimeCorrections.h"

#include <limits>
#include <string>

#include <Eigen/Core>

#include "Tudat/Astrodynamics/BasicAstrodynamics/physicalConstants.h"
#include "Tudat/Basics/testMacros.h"

#include "Tudat/External/SpiceInterface/spiceEphemeris.h"
#include "Tudat/External/SpiceInterface/spiceInterface.h"
#include "Tudat/InputOutput/basicInputOutput.h"

namespace tudat
{
namespace unit_tests
{

using namespace ephemerides;
using namespace observation_models;
using namespace spice_interface;

BOOST_AUTO_TEST_SUITE( test_light_time )

//! Test light-time calculator.
BOOST_AUTO_TEST_CASE( testLightWithSpice )
{
    // Load spice kernels.
    const std::string kernelsPath = input_output::getSpiceKernelPath( );
    loadSpiceKernelInTudat( kernelsPath + "pck00009.tpc" );
    loadSpiceKernelInTudat( kernelsPath + "de-403-masses.tpc" );
    loadSpiceKernelInTudat( kernelsPath + "de421.bsp" );
    loadSpiceKernelInTudat( kernelsPath + "naif0009.tls" );

    // Define names of bodies and frames.
    const std::string earth = "Earth";
    const std::string moon = "Moon";
    const std::string frame = "ECLIPJ2000";

    // Create ephemerides of Earth and Moon, with data from Spice.
    boost::shared_ptr< SpiceEphemeris > earthEphemeris = boost::make_shared< SpiceEphemeris >(
                earth, "SSB", false, false, false, frame );
    boost::shared_ptr< SpiceEphemeris > moonEphemeris = boost::make_shared< SpiceEphemeris >(
                moon, "SSB", false, false, false, frame );

    // Create light-time calculator, Earth center transmitter, Moon center receiver.
    boost::shared_ptr< LightTimeCalculator > lightTimeEarthToMoon =
            boost::make_shared< LightTimeCalculator >
            ( boost::bind( &Ephemeris::getCartesianStateFromEphemeris, earthEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              boost::bind( &Ephemeris::getCartesianStateFromEphemeris, moonEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ) );

    // Define input time for tests.
    const double testTime = 1.0E6;

    // Define Spice output variables.
    double spiceOutputState[ 6 ] = { };
    double spiceMoonLightTime = 0.0;

    // Calculate observed (i.e. relative) position of Earth, and 'light time' at 'testTime' on
    // Moon, using spice. (Reception case with converged Newtonian light-time correction.)
    spkezr_c( earth.c_str( ), testTime, frame.c_str( ), std::string( "CN" ).c_str( ),
              moon.c_str( ), spiceOutputState, &spiceMoonLightTime );
    Eigen::Vector3d spiceMoonToEarthVector = Eigen::Vector3d::Zero( );
    for( int i = 0; i < 3; i++ )
    {
        // Convert from kilometers to meters.
        spiceMoonToEarthVector( i ) = -1000.0 * spiceOutputState[ i ];
    }

    // Calculate light time, with as input time the reception time, using light-time calculator.
    double testMoonLightTime = lightTimeEarthToMoon->calculateLightTime( testTime, true );
    BOOST_CHECK_CLOSE_FRACTION( testMoonLightTime, spiceMoonLightTime, 1.0E-9 );

    // Calculate relativeRange vector, with as input time the reception time, using light-time
    // calculator.
    const Eigen::Vector3d testMoonToEarthVector =
            lightTimeEarthToMoon->calculateRelativeRangeVector( testTime, true );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION( testMoonToEarthVector, spiceMoonToEarthVector, 1.0E-12 );

    // Calculate observed (i.e. relative) position of Earth, and 'light time' at
    // 'testTime+light time' on Moon, using spice. (Transmission case with converged Newtonian
    // light-time correction.)
    spkezr_c( moon.c_str( ), testTime, frame.c_str( ), std::string( "XCN" ).c_str( ),
              earth.c_str( ), spiceOutputState, &spiceMoonLightTime );
    Eigen::Vector3d spiceEarthToMoonVector = Eigen::Vector3d::Zero( );
    for( int i = 0; i < 3; i++ )
    {
        // Convert from kilometers to meters.
        spiceEarthToMoonVector( i ) = 1000.0 * spiceOutputState[ i ];
    }

    // Calculate light time, with as input time the transmission time, using light-time calculator.
    testMoonLightTime = lightTimeEarthToMoon->calculateLightTime( testTime, false );
    BOOST_CHECK_CLOSE_FRACTION( testMoonLightTime, spiceMoonLightTime, 1.0E-9 );

    // Calculate relativeRange vector, with as input time the transmission time, using light-time
    // calculator.
    const Eigen::Vector3d testEarthToMoonVector =
            lightTimeEarthToMoon->calculateRelativeRangeVector( testTime, false );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION( testEarthToMoonVector, spiceEarthToMoonVector, 1.0E-10 );

    // Test light time and link end state functions.
    double testOutputTime = 0.0;
    basic_mathematics::Vector6d testEarthState = basic_mathematics::Vector6d::Zero( );
    basic_mathematics::Vector6d testMoonState = basic_mathematics::Vector6d::Zero( );
    basic_mathematics::Vector6d spiceEarthState = basic_mathematics::Vector6d::Zero( );
    basic_mathematics::Vector6d spiceMoonState = basic_mathematics::Vector6d::Zero( );

    // Get link end states, assuming input time is transmission time.
    // SSB = Solar system barycenter.
    testOutputTime = lightTimeEarthToMoon->calculateLightTimeWithLinkEndsStates(
                testMoonState, testEarthState, testTime, false );
    spiceEarthState = getBodyCartesianStateAtEpoch( earth, "SSB", "ECLIPJ2000", "NONE", testTime );
    spiceMoonState = getBodyCartesianStateAtEpoch(
                moon, "SSB", "ECLIPJ2000", "NONE", testTime + testOutputTime );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION(
                spiceEarthState, testEarthState, std::numeric_limits< double >::epsilon( ) );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION(
                spiceMoonState, testMoonState, std::numeric_limits< double >::epsilon( ) );

    // Get link end states, assuming input time is reception time.
    testOutputTime = lightTimeEarthToMoon->calculateLightTimeWithLinkEndsStates(
                testMoonState, testEarthState, testTime, true );
    spiceEarthState = getBodyCartesianStateAtEpoch( earth, "SSB", "ECLIPJ2000", "NONE",
                                                    testTime - testOutputTime );
    spiceMoonState = getBodyCartesianStateAtEpoch( moon, "SSB", "ECLIPJ2000", "NONE", testTime );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION(
                spiceEarthState, testEarthState, std::numeric_limits< double >::epsilon( ) );
    TUDAT_CHECK_MATRIX_CLOSE_FRACTION(
                spiceMoonState, testMoonState, std::numeric_limits< double >::epsilon( ) );

    // Test light time with corrections.

    // Set single light-time correction function.
    std::vector< LightTimeCorrectionFunction > lightTimeCorrections;
    lightTimeCorrections.push_back( &getTimeDifferenceLightTimeCorrection );

    // Create light-time object with correction.
    boost::shared_ptr< LightTimeCalculator > lightTimeEarthToMoonWithCorrection =
            boost::make_shared< LightTimeCalculator >
            ( boost::bind( &Ephemeris::getCartesianStateFromEphemeris, earthEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              boost::bind( &Ephemeris::getCartesianStateFromEphemeris, moonEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              lightTimeCorrections, true );

    // Calculate newtonian light time.
    double newtonianLightTime = lightTimeEarthToMoonWithCorrection->calculateRelativeRangeVector(
                testTime, true ).norm( ) / physical_constants::SPEED_OF_LIGHT;

    // Calculate light time (including correction), at reception.
    testMoonLightTime = lightTimeEarthToMoonWithCorrection->calculateLightTime( testTime, true );

    // Calculate expected correction.
    double expectedCorrection = getTimeDifferenceLightTimeCorrection(
                basic_mathematics::Vector6d::Zero( ), basic_mathematics::Vector6d::Zero( ),
                testTime - testMoonLightTime, testTime );

    // Test whether results are approximately equal.
    BOOST_CHECK_CLOSE_FRACTION( newtonianLightTime + expectedCorrection,
                                testMoonLightTime,
                                1E-14 );

    // Create light-time object with correction, without iterating light-time corrections.
    lightTimeEarthToMoonWithCorrection =
            boost::make_shared< LightTimeCalculator >
            ( boost::bind( &Ephemeris::getCartesianStateFromEphemeris, earthEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              boost::bind( &Ephemeris::getCartesianStateFromEphemeris, moonEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              lightTimeCorrections, false );

    // Calculate newtonian light time.
    newtonianLightTime = lightTimeEarthToMoonWithCorrection->calculateRelativeRangeVector(
                testTime, true ).norm( ) / physical_constants::SPEED_OF_LIGHT;

    // Calculate light time (including correction), at reception.
    testMoonLightTime = lightTimeEarthToMoonWithCorrection->calculateLightTime( testTime, true );

    // Calculate expected correction.
    expectedCorrection = getTimeDifferenceLightTimeCorrection(
                basic_mathematics::Vector6d::Zero( ), basic_mathematics::Vector6d::Zero( ),
                testTime - testMoonLightTime, testTime );

    // Test whether results are approximately equal.
    BOOST_CHECK_CLOSE_FRACTION( newtonianLightTime + expectedCorrection,
                                testMoonLightTime,
                                1E-14 );

    // Add two more light-time corrections.
    lightTimeCorrections.push_back( &getVelocityDifferenceLightTimeCorrection );
    lightTimeCorrections.push_back( &getPositionDifferenceLightTimeCorrection );

    // Create light-time object with multiple corrections.
    lightTimeEarthToMoonWithCorrection =
            boost::make_shared< LightTimeCalculator >
            ( boost::bind( &Ephemeris::getCartesianStateFromEphemeris, earthEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              boost::bind( &Ephemeris::getCartesianStateFromEphemeris, moonEphemeris, _1,
                           basic_astrodynamics::JULIAN_DAY_ON_J2000 ),
              lightTimeCorrections, true );

    // Calculate newtonian light time.
    newtonianLightTime = lightTimeEarthToMoonWithCorrection->calculateRelativeRangeVector(
                testTime, true ).norm( ) / physical_constants::SPEED_OF_LIGHT;

    // Calculate light time (including corrections), at reception.
    testMoonLightTime = lightTimeEarthToMoonWithCorrection->calculateLightTimeWithLinkEndsStates(
                testMoonState, testEarthState, testTime, true );

    // Calculate and sum expected correction.
    expectedCorrection = getTimeDifferenceLightTimeCorrection(
                testEarthState, testMoonState, testTime - testMoonLightTime, testTime );
    expectedCorrection += getPositionDifferenceLightTimeCorrection(
                testEarthState, testMoonState, testTime - testMoonLightTime, testTime );
    expectedCorrection += getVelocityDifferenceLightTimeCorrection(
                testEarthState, testMoonState, testTime - testMoonLightTime, testTime );

    // Test whether results are approximately equal.
    BOOST_CHECK_CLOSE_FRACTION( newtonianLightTime + expectedCorrection,
                                testMoonLightTime,
                                1E-14 );
}

BOOST_AUTO_TEST_SUITE_END( )

} // namespace unit_tests
} // namespace tudat
