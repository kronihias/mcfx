/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "ConvolverData.h"

#include "resampler.h"

ConvolverData::ConvolverData()
{
    NumInCh = 0;
    NumOutCh = 0;
    MaxLen = 0;
    SampleRate = 44100.f;
}

ConvolverData::~ConvolverData()
{

}

void ConvolverData::clear()
{
    NumInCh = 0;
    NumOutCh = 0;
    MaxLen = 0;

    IR.clear();
}

int ConvolverData::getNumInputChannels()
{
    return NumInCh + 1;
}

int ConvolverData::getNumOutputChannels()
{
    return NumOutCh + 1;
}

int ConvolverData::getNumIRs()
{
    return IR.size();
}


void ConvolverData::addIR(int in_ch, int out_ch, int offset, int delay, int length, AudioBuffer<float>& buffer, int buffer_ch, double src_samplerate)
{
    if (delay < 0)
        delay = 0;
    if (offset < 0)
        offset = 0;

    if ( (length <= 0) || (length+offset > buffer.getNumSamples()) )
        length = buffer.getNumSamples()-offset;

    IR.add(new IR_Data);
    IR.getLast()->InCh = in_ch;
    IR.getLast()->OutCh = out_ch;

    AudioBuffer<float> *IRBuf = &IR.getLast()->Data;

    const int size = length + delay; // absolute length of ir

    if (src_samplerate == SampleRate) {
        IRBuf->setSize(1, size);
        IRBuf->clear();
        IRBuf->copyFrom(0, delay, buffer, buffer_ch, offset, length); // copy the wanted part
    } else {
        // resampling path

        const double ratio = SampleRate / src_samplerate; // output / input rate

        const double lowpassRatio = (ratio < 1.0) ? ratio : 1.0;
        const int num_filters = 512;
        const int num_taps = 512;

        const int input_latency = num_taps / 2;
        const int output_latency = ceil(input_latency * ratio);
        const int inputsize_zeropadded = size + input_latency;

        AudioBuffer<float> sourceBuffer(1, inputsize_zeropadded);
        sourceBuffer.clear();
        sourceBuffer.copyFrom(0, delay, buffer, buffer_ch, offset, length); // copy the wanted part

        const int newsize = (int)ceil(size * ratio) + output_latency;
        AudioBuffer<float> resampledBuffer(1, newsize);
        resampledBuffer.clear();

        Resample *resampler = resampleInit (1, num_taps, num_filters, lowpassRatio, BLACKMAN_HARRIS | INCLUDE_LOWPASS); // SUBSAMPLE_INTERPOLATE
        auto result = resampleProcess(resampler, sourceBuffer.getArrayOfReadPointers(), inputsize_zeropadded, resampledBuffer.getArrayOfWritePointers(), newsize, ratio);
        resampleFree(resampler);

        resampledBuffer.applyGain(1. / ratio);

        auto val_1 = sourceBuffer.getMagnitude(0, 0, inputsize_zeropadded);
        auto val_2 = resampledBuffer.getMagnitude(0, 0, newsize);

        IRBuf->setSize(1, newsize);
        IRBuf->clear();
        IRBuf->copyFrom(0, 0, resampledBuffer, 0, output_latency, newsize - output_latency); // we truncate the pre-ringing / latency - this causes issues if IRs have a dirac in the beginning!
# if 0
        const double factorWriting = SampleRate / src_samplerate;
        const double factorReading = src_samplerate / SampleRate;

        juce::WindowedSincInterpolator interpolator;
        //juce::LagrangeInterpolator interpolator;

        const int resampler_input_latency = roundToInt(interpolator.getBaseLatency());
        const int resampler_output_latency = roundToInt(interpolator.getBaseLatency() / factorReading);

        const int inputsize_zeropadded = size + 2*resampler_input_latency;
        
        const int newsize = (int)ceil(size * factorWriting) + resampler_output_latency;

        AudioBuffer<float> sourceBuffer(1, inputsize_zeropadded);
        sourceBuffer.clear();
        sourceBuffer.copyFrom(0, delay, buffer, buffer_ch, offset, length); // copy the wanted part

        AudioBuffer<float> resampledBuffer(1, newsize + resampler_output_latency);

        auto used_samples = interpolator.process(factorReading, sourceBuffer.getReadPointer(0, 0), resampledBuffer.getWritePointer(0, 0), newsize + resampler_output_latency);

        auto val_1 = sourceBuffer.getMagnitude(0, 0, inputsize_zeropadded);
        auto val_2 = resampledBuffer.getMagnitude(0, 0, newsize + resampler_output_latency);

        IRBuf->setSize(1, newsize);
        IRBuf->clear();
        IRBuf->copyFrom(0, 0, resampledBuffer, 0, resampler_output_latency, newsize); // we truncate the pre-ringing / latency - this causes issues if IRs have a dirac in the beginning!
#endif
    }

    NumInCh = (NumInCh < in_ch) ? in_ch : NumInCh;
    NumOutCh = (NumOutCh < out_ch) ? out_ch : NumOutCh;

    if (IRBuf->getNumSamples() > MaxLen)
        MaxLen = IRBuf->getNumSamples();
}


AudioSampleBuffer* ConvolverData::getIR(int id)
{
    if (id < IR.size()) {
        return &IR.getUnchecked(id)->Data;
    } else
        return 0;
}

int ConvolverData::getInCh(int id)
{
    if (id < IR.size()) {
        return IR.getUnchecked(id)->InCh;
    } else
        return 0;
}

int ConvolverData::getOutCh(int id)
{
    if (id < IR.size()) {
        return IR.getUnchecked(id)->OutCh;
    } else
        return 0;
}

int ConvolverData::getLength(int id)
{
    if (id < IR.size()) {
        return IR.getUnchecked(id)->Data.getNumSamples();
    } else
        return 0;
}

int ConvolverData::getMaxLength()
{
    return MaxLen;
}

double ConvolverData::getMaxLengthInSeconds()
{
    return (double)MaxLen/SampleRate;
}

void ConvolverData::setSampleRate(double samplerate)
{
    if (samplerate > 0.f)
        SampleRate = samplerate;
}
