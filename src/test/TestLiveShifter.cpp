/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2024 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#ifndef BOOST_TEST_DYN_LINK
#define BOOST_TEST_DYN_LINK
#endif
#include <boost/test/unit_test.hpp>

#include "../../rubberband/RubberBandLiveShifter.h"

#include <iostream>
#include <fstream>
#include <string>

#include <cmath>

using namespace RubberBand;

using std::vector;
using std::cerr;
using std::endl;
using std::string;
using std::ofstream;

namespace tt = boost::test_tools;

BOOST_AUTO_TEST_SUITE(TestLiveShifter)

static void dumpTo(string basename,
                   const vector<float> &data)
{
    string dir = "/tmp";
    string filename = dir + "/" + basename + ".csv";
    ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file) {
        cerr << "dumpTo: failed to open file \"" << filename << "\" for writing" << endl;
        return;
    }
    file << "sample,V" << endl;
    for (int i = 0; i < int(data.size()); ++i) {
        file << i << "," << data[i] << endl;
    }
}

static void dump(string prefix,
                 const vector<float> &in,
                 const vector<float> &out,
                 const vector<float> &expected,
                 int delay)
{
    cerr << "dump: delay reported as " << delay << endl;

    if (prefix != "") {
        prefix += "-";
    }
    
    dumpTo(prefix + "in", in);
    dumpTo(prefix + "out", out);
    dumpTo(prefix + "expected", expected);

    vector<float> shifted;
    vector<float> diff;
    for (int i = 0; i + delay < int(out.size()); ++i) {
        shifted.push_back(out[i + delay]);
        diff.push_back(out[i + delay] - expected[i]);
    }
    dumpTo(prefix + "shifted", shifted);
    dumpTo(prefix + "diff", diff);
}

static void check_sinusoid_unchanged(int n, int rate, float freq,
                                     RubberBandLiveShifter::Options options,
                                     string debugPrefix = {})
{
    bool printDebug = (debugPrefix != "");
    
    if (printDebug) {
        RubberBandLiveShifter::setDefaultDebugLevel(2);
    }
    
    RubberBandLiveShifter shifter(rate, 1, options);
    
    int blocksize = shifter.getBlockSize();
    BOOST_TEST(blocksize == 512);

    n = (n / blocksize + 1) * blocksize;
    
    vector<float> in(n), out(n);
    for (int i = 0; i < n; ++i) {
        in[i] = 0.5f * sinf(float(i) * freq * M_PI * 2.f / float(rate));
    }

    for (int i = 0; i < n; i += blocksize) {
        float *inp = in.data() + i;
        float *outp = out.data() + i;
        shifter.shift(&inp, &outp);
    }

    int delay = shifter.getStartDelay();
    
    // We now have n samples of a simple sinusoid with stretch factor
    // 1.0; obviously we expect the output to be essentially the same
    // thing. It will have lower precision for a while at the start,
    // so we check that with a threshold of 0.1; after that we expect
    // better precision.

    int slackpart = 2048;
    float slackeps = 1.0e-1f;
    float eps = 1.0e-3f;

#ifdef USE_BQRESAMPLER
    eps = 1.0e-2f;
#endif
    
    for (int i = 0; i < slackpart; ++i) {
        float fin = in[i];
        float fout = out[delay + i];
        float err = fabsf(fin - fout);
        if (err > slackeps) {
            cerr << "Error at index " << i << " exceeds slack eps "
                 << slackeps << ": output " << fout << " - input "
                 << fin << " = " << fout - fin << endl;
            BOOST_TEST(err < eps);
            break;
        }
    }
    
    for (int i = slackpart; i < n - delay; ++i) {
        float fin = in[i];
        float fout = out[delay + i];
        float err = fabsf(fin - fout);
        if (err > eps) {
            cerr << "Error at index " << i << " exceeds tight eps "
                 << eps << ": output " << fout << " - input "
                 << fin << " = " << fout - fin << endl;
            BOOST_TEST(err < eps);
            break;
        }
    }

    if (printDebug) {
        RubberBandLiveShifter::setDefaultDebugLevel(0);
        dump(debugPrefix, in, out, in, delay);
    }
}

static void check_sinusoid_shifted(int n, int rate, float freq, float shift,
                                   RubberBandLiveShifter::Options options,
                                   string debugPrefix = {})
{
    bool printDebug = (debugPrefix != "");
    
    if (printDebug) {
        RubberBandLiveShifter::setDefaultDebugLevel(2);
    }
    
    RubberBandLiveShifter shifter(rate, 1, options);

    shifter.setPitchScale(shift);
    
    int blocksize = shifter.getBlockSize();
    BOOST_TEST(blocksize == 512);

    n = (n / blocksize + 1) * blocksize;
    
    vector<float> in(n), out(n), expected(n);
    int endpoint = n;
    if (endpoint > 20000) endpoint -= 10000;
    double sumSquares = 0;
    double peakIn = 0;
    for (int i = 0; i < n; ++i) {
        float value = 0.5f * sinf(float(i) * freq * M_PI * 2.f / float(rate));
        if (i < endpoint) {
            sumSquares += value * value;
        }
        if (i > endpoint && value > 0.f && in[i-1] <= 0.f) break;
        if (fabs(value) > peakIn) {
            peakIn = fabs(value);
        }
        in[i] = value;
        expected[i] = 0.5f * sinf(float(i) * freq * shift * M_PI * 2.f / float(rate));
    }
    double rmsIn = sqrt(sumSquares / endpoint);
    cerr << "rmsIn = " << rmsIn << endl;
    cerr << "peakIn = " << peakIn << endl;

    for (int i = 0; i < n; i += blocksize) {
        float *inp = in.data() + i;
        float *outp = out.data() + i;
        shifter.shift(&inp, &outp);
    }

    int reportedDelay = shifter.getStartDelay();

    double lastCrossing = -1;
    int nCrossings = 0;
    double accWavelength = 0;
    double minWavelength = 0;
    double maxWavelength = 0;
    sumSquares = 0;
    double peakOut = 0;
    for (int i = reportedDelay; i < endpoint; ++i) {
        sumSquares += out[i] * out[i];
        if (fabs(out[i]) > peakOut) peakOut = fabs(out[i]);
        if (out[i-1] < 0.f && out[i] >= 0.f) {
            double crossing = (i-1) + (out[i-1] / (out[i-1] - out[i]));
            if (lastCrossing >= 0) {
                double wavelength = crossing - lastCrossing;
                accWavelength += wavelength;
                if (minWavelength == 0 || wavelength < minWavelength) {
                    minWavelength = wavelength;
                }
                if (maxWavelength == 0 || wavelength > maxWavelength) {
                    maxWavelength = wavelength;
                }
                cerr << "wavelength = " << wavelength << " (freq " << rate / wavelength << ")" << endl;
                nCrossings ++;
            }
            lastCrossing = crossing;
        }
    }
    cerr << endl;

    double avgWavelength = 1;
    if (nCrossings > 0) {
        avgWavelength = accWavelength / nCrossings;
    }
    double detectedFreq = double(rate) / double(avgWavelength);
    cerr << "nCrossings = " << nCrossings << ", minWavelength = " << minWavelength << ", maxWavelength = " << maxWavelength << ", avgWavelength = " << avgWavelength << ", detectedFreq = " << detectedFreq << " (expected " << freq * shift << ")" << endl;
    
    double rms = sqrt(sumSquares / (endpoint - reportedDelay));
    cerr << "rms = " << rms << endl;
    cerr << "peak = " << peakOut << endl;
    
    int slackpart = 2048;
    int delay = reportedDelay + slackpart;
    
    // Align to the next zero-crossing in output, as phase may differ
    
    for (int i = delay; i < endpoint; ++i) {
        if (out[i] < 0.f && out[i+1] >= 0.f) {
            cerr << "zc: at " << i << " we have " << out[i] << ", " << out[i+1] << endl;
            delay = i+1;
            break;
        }
    }

    cerr << "Adjusted delay from reported value of " << reportedDelay
         << " by adding slack of " << slackpart
         << " and moving to next positive zero crossing at " << delay << endl;

    float eps = 1.0e-3f;
    
#ifdef USE_BQRESAMPLER
    eps = 1.0e-2f;
#endif
    
    for (int i = 0; i + delay < endpoint; ++i) {
        float fin = expected[i];
        float fout = out[delay + i];
        float err = fabsf(fin - fout);
        if (err > eps) {
            cerr << "Error at index " << i << " exceeds eps "
                 << eps << ": output " << fout << " - expected "
                 << fin << " = " << fout - fin << endl;
            BOOST_TEST(err < eps);
            break;
        }
    }

    if (printDebug) {
        RubberBandLiveShifter::setDefaultDebugLevel(0);
        dump(debugPrefix, in, out, expected, delay);
    }
}

BOOST_AUTO_TEST_CASE(sinusoid_unchanged)
{
    int n = 20000;

    // delay = 2112, correct
    
    check_sinusoid_unchanged(n, 44100, 440.f, 0, "unchanged-440");
    check_sinusoid_unchanged(n, 48000, 260.f, 0);
}

BOOST_AUTO_TEST_CASE(sinusoid_down_octave_440)
{
    // Checked: delay = 3648, correct

    // or about 3160?
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 0.5f, 0, "down-octave-440");
}

BOOST_AUTO_TEST_CASE(sinusoid_down_octave_260)
{
    // Checked: delay = 3648, correct

    int n = 30000;
    check_sinusoid_shifted(n, 48000, 260.f, 0.5f, 0);
}

BOOST_AUTO_TEST_CASE(sinusoid_down_2octave)
{
    // Checked: delay = 6784, sound

    // I like about 5250
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 0.25f, 0, "down-2octave-440");
//    check_sinusoid_shifted(n, 48000, 260.f, 0.25f, 0);
}

BOOST_AUTO_TEST_CASE(sinusoid_up_octave_440)
{
    // Checked: delay = 2879, correct
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 2.0f, 0);
}

BOOST_AUTO_TEST_CASE(sinusoid_up_octave_260)
{
    // Checked: delay = 2879, correct

    //!!! or 3380?
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 260.f, 2.0f, 0, "up-octave-260");
}

BOOST_AUTO_TEST_CASE(sinusoid_up_2octave)
{
    // Checked: delay = 3006 -> highly implausible, must be higher
    // 3670 ish?
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 4.0f, 0, "up-2octave-440");
    check_sinusoid_shifted(n, 48000, 260.f, 4.0f, 0);
}

BOOST_AUTO_TEST_CASE(sinusoid_down_0_99)
{
    
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 0.99f, 0, "down-0_99-440");
}

BOOST_AUTO_TEST_CASE(sinusoid_up_1_01)
{
    int n = 30000;
    check_sinusoid_shifted(n, 44100, 440.f, 1.01f, 0, "up-1_01-440");
}

BOOST_AUTO_TEST_SUITE_END()
