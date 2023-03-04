/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#ifndef PLUGINPROCESSOR_H_INCLUDED
#define PLUGINPROCESSOR_H_INCLUDED

#include "JuceHeader.h"

#include "FilterInfo.h"
#include "SmoothIIRFilter.h"

#include <complex>

#if JUCE_MAC
    #define VIMAGE_H // avoid namespace clashes
    #include <Accelerate/Accelerate.h>

    #define SPLIT_COMPLEX 1
#else
    #include <fftw3.h>
    const int fftwopt = FFTW_MEASURE; // FFTW_ESTIMATE || FFTW_MEASURE

    #define SPLIT_COMPLEX 0
#endif

// Aligned Memory Allocation and Deallocation

inline void* aligned_malloc(size_t size, size_t align) {
    void *result;
#ifdef _MSC_VER
    result = _aligned_malloc(size, align);
#else
    if(posix_memalign(&result, align, size)) result = 0;
#endif
    return result;
}

inline void aligned_free(void *ptr) {
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif

}

inline float LOG2F_( float n )
{
    // log(n)/log(2) is log2.
    return logf( n ) / logf( 2 );
}

//==============================================================================
/**
*/

// convert param 0...1 to freq 24Hz ... 21618Hz
inline float param2freq(float param)
{
    return powf(2.f, param*9.8f + 4.6f);
}

inline float freq2param(float freq)
{
    return (LOG2F_(freq)-4.6f)/9.8f;
}

// convert parameter 0...1 to Q 0.2 .... 20
inline float param2q(float param)
{
    return powf(2.f, param*6.6439f)*0.2f;
}

inline float q2param(float q)
{
    return LOG2F_(q/0.2f)/6.6439f;
}

inline float param2db(float param)
{
    return param*36.f-18.f;
}

inline float db2param(float db)
{
    return (db+18.f)/36.f;
}

#define LOGTEN 2.302585092994
inline float dbtorms(float db)
{
    return expf(((float)LOGTEN * 0.05f) * db);
}

inline float param2gain (float param)
{
    return dbtorms(param2db(param));
}

#define FFT_LENGTH 4096

class LowhighpassAudioProcessor  : public AudioProcessor,
                                   public FilterInfo,
                                   public ChangeBroadcaster
{
public:
    //==============================================================================
    LowhighpassAudioProcessor();
    ~LowhighpassAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;

    void checkFilters(bool force_update);

    freqResponse getResponse(double f) override;

    float inMagnitude(double f) override;
    float outMagnitude(double f) override;

    void freqanalysis(bool activate);

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    int getNumParameters() override;

    float getParameter (int index) override;
    void setParameter (int index, float newValue) override;

    const String getParameterName (int index) override;
    const String getParameterText (int index) override;

    const String getInputChannelName (int channelIndex) const override;
    const String getOutputChannelName (int channelIndex) const override;
    bool isInputChannelStereoPair (int index) const override;
    bool isOutputChannelStereoPair (int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool silenceInProducesSilenceOut() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

  enum Parameters
	{
    LCOnParam,
    LCfreqParam,
    LCorderParam,

    HCOnParam,
    HCfreqParam,
    HCorderParam,

    PF1GainParam,
    PF1freqParam,
    PF1QParam,

    PF2GainParam,
    PF2freqParam,
    PF2QParam,

    LSGainParam,
    LSfreqParam,
    LSQParam,

    HSGainParam,
    HSfreqParam,
    HSQParam,

		totalNumParams
	};

    bool _freqanalysis; // compute the spectrum of the summed input/output signals
    bool _editorOpen; // editor is open

private:

    double _sampleRate;

    float _lc_on_param, // 1 = on
    _lc_freq_param, // 0...20kHz log
    _lc_order_param,
    _lc_freq_param_; // 1... 2x2nd order butterworth cascaded, 0... 1x2nd order butterworth
    float _hc_on_param, _hc_freq_param, _hc_order_param,_hc_freq_param_;

    float _pf1_gain_param, _pf1_freq_param, _pf1_q_param;
    float _pf2_gain_param, _pf2_freq_param, _pf2_q_param;

    float _ls_gain_param, _ls_freq_param, _ls_q_param;
    float _hs_gain_param, _hs_freq_param, _hs_q_param;

    float _pf1_gain_param_, _pf1_freq_param_, _pf1_q_param_; // buffer
    float _pf2_gain_param_, _pf2_freq_param_, _pf2_q_param_;


    float _ls_gain_param_, _ls_freq_param_, _ls_q_param_;
    float _hs_gain_param_, _hs_freq_param_, _hs_q_param_;

    OwnedArray<SmoothIIRFilter> _LC_IIR_1;
    OwnedArray<SmoothIIRFilter> _LC_IIR_2;
    OwnedArray<SmoothIIRFilter> _HC_IIR_1;
    OwnedArray<SmoothIIRFilter> _HC_IIR_2;


    IIRCoefficients _IIR_LC_Coeff;
    IIRCoefficients _IIR_HC_Coeff;

    // Peak/Notch Filters
    IIRCoefficients _IIR_PF_Coeff_1;
    OwnedArray<SmoothIIRFilter> _PF_IIR_1;


    IIRCoefficients _IIR_PF_Coeff_2;
    OwnedArray<SmoothIIRFilter> _PF_IIR_2;


    // High/Low Shelf

    IIRCoefficients _IIR_LS_Coeff;
    OwnedArray<SmoothIIRFilter> _LS_IIR;


    IIRCoefficients _IIR_HS_Coeff;
    OwnedArray<SmoothIIRFilter> _HS_IIR;

    AudioSampleBuffer _analysis_inbuf, _analysis_outbuf;
    float *_in_mag, *_out_mag; // magnitude frequency
    float *_w; // window function

    int _bufpos;

    float               *fft_t_;             // time data for fft/ifft -> 2*N

#if SPLIT_COMPLEX
    FFTSetup            vdsp_fft_setup_;     // Apple vDSP FFT plan
    DSPSplitComplex     splitcomplex_;

    float               *fft_re_;            // N+1
    float               *fft_im_;            // N+1
    int                 vdsp_log2_;      // vDSP needs the exponent of two
#else
    fftwf_plan          fftwf_plan_r2c_;     // FFTWF forward plan
    float               *fftwf_t_data_;      // FFTWF buffer for time domain signal (2*N)
    fftwf_complex       *fft_c_;             // FFTWF buffer for complex signal (N+1)
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowhighpassAudioProcessor)
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
