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

#include "PluginProcessor.h"
#include "PluginEditor.h"

#define _USE_MATH_DEFINES
#include <math.h>

//==============================================================================
LowhighpassAudioProcessor::LowhighpassAudioProcessor() :
    AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels(NUM_CHANNELS), true)
    ),
    _freqanalysis(false),
    _editorOpen(false),
    _lc_on_param(0.f),
    _lc_freq_param(0.1f),
    _lc_order_param(1.f),
    _hc_on_param(0.f),
    _hc_freq_param(0.9f),
    _hc_order_param(1.f),
    _pf1_gain_param(0.5f),
    _pf1_freq_param(0.3f),
    _pf1_q_param(0.27f), // Q=0.7
    _pf2_gain_param(0.5f),
    _pf2_freq_param(0.6f),
    _pf2_q_param(0.27f), // Q=0.7
    _ls_gain_param(0.5f),
    _ls_freq_param(0.2f),
    _ls_q_param(0.27f), // Q=0.7
    _hs_gain_param(0.5f),
    _hs_freq_param(0.7f),
    _hs_q_param(0.27f), // Q=0.7
    _analysis_inbuf(1, FFT_LENGTH),
    _analysis_outbuf(1, FFT_LENGTH),
    _bufpos(0)
{

    // initiate and allocate fft
    fft_t_ = reinterpret_cast<float*>( aligned_malloc( FFT_LENGTH*sizeof(float), 16 ) );
    FloatVectorOperations::clear(&fft_t_[0], FFT_LENGTH);

#if SPLIT_COMPLEX

    fft_re_ = reinterpret_cast<float*>( aligned_malloc( (FFT_LENGTH/2+1)*sizeof(float), 16 ) );
    fft_im_ = reinterpret_cast<float*>( aligned_malloc( (FFT_LENGTH/2+1)*sizeof(float), 16 ) );

    splitcomplex_.realp = fft_re_;
    splitcomplex_.imagp = fft_im_;

    vdsp_log2_ = 0;
    while ((1 << vdsp_log2_) < FFT_LENGTH)
    {
        ++vdsp_log2_; // N=2^vdsp_log2_
    }

    vdsp_fft_setup_ = vDSP_create_fftsetup(vdsp_log2_, FFT_RADIX2);

#else

    fft_c_ = reinterpret_cast<fftwf_complex*>( aligned_malloc( (FFT_LENGTH/2+1)*sizeof(fftwf_complex), 16 ) );

    FloatVectorOperations::clear(&fft_c_[0][0], 2*(FFT_LENGTH/2+1));

    fftwf_plan_r2c_ = nullptr;

    fftwf_make_planner_thread_safe(); // make plan creation threadsafe - new in fftw-3.3.6-pl2

    fftwf_plan_r2c_ = fftwf_plan_dft_r2c_1d (FFT_LENGTH, fft_t_, fft_c_, fftwopt);
#endif

    _w = reinterpret_cast<float*>( aligned_malloc( FFT_LENGTH*sizeof(float), 16 ) );

//    // init hann window
//    for (int i=0; i<FFT_LENGTH; i++) {
//        _w[i] = 0.5f*(1.f-cosf(2.f*M_PI*i/FFT_LENGTH));
//    }

    // init blackman-harris window
    for (int i=0; i<FFT_LENGTH; i++) {
        _w[i] = 0.35875f - 0.48829*cosf(2.f*M_PI*i/(FFT_LENGTH-1)) + 0.14128*cosf(4.f*M_PI*i/(FFT_LENGTH-1)) - 0.01168*cosf(6.f*M_PI*i/(FFT_LENGTH-1));
    }

    _in_mag = reinterpret_cast<float*>( aligned_malloc( (FFT_LENGTH/2+1)*sizeof(float), 16 ) );
    _out_mag = reinterpret_cast<float*>( aligned_malloc( (FFT_LENGTH/2+1)*sizeof(float), 16 ) );

    FloatVectorOperations::clear(_in_mag, FFT_LENGTH/2+1);
    FloatVectorOperations::clear(_out_mag, FFT_LENGTH/2+1);

}

LowhighpassAudioProcessor::~LowhighpassAudioProcessor()
{
#if SPLIT_COMPLEX
    if (vdsp_fft_setup_)
        vDSP_destroy_fftsetup(vdsp_fft_setup_);

    aligned_free(fft_re_);
    aligned_free(fft_im_);

#else
    if (fftwf_plan_r2c_)
        fftwf_destroy_plan(fftwf_plan_r2c_);

    aligned_free(fft_c_);
#endif

    aligned_free(fft_t_);

    aligned_free(_in_mag);
    aligned_free(_out_mag);
    aligned_free(_w);

}

//==============================================================================
const String LowhighpassAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int LowhighpassAudioProcessor::getNumParameters()
{
    return totalNumParams;
}

float LowhighpassAudioProcessor::getParameter (int index)
{
    switch (index)
	{
		case LCOnParam:    return _lc_on_param;
		case LCfreqParam:  return _lc_freq_param;
		case LCorderParam: return _lc_order_param;

		case HCOnParam:      return _hc_on_param;
		case HCfreqParam:    return _hc_freq_param;
		case HCorderParam:   return _hc_order_param;

    case PF1GainParam:  return _pf1_gain_param;
    case PF1freqParam:  return _pf1_freq_param;
    case PF1QParam:     return _pf1_q_param;

    case PF2GainParam:  return _pf2_gain_param;
    case PF2freqParam:  return _pf2_freq_param;
    case PF2QParam:     return _pf2_q_param;

    case LSGainParam:  return _ls_gain_param;
    case LSfreqParam:  return _ls_freq_param;
    case LSQParam:     return _ls_q_param;

    case HSGainParam:  return _hs_gain_param;
    case HSfreqParam:  return _hs_freq_param;
    case HSQParam:     return _hs_q_param;

		default:            return 0.0f;
	}
}

void LowhighpassAudioProcessor::setParameter (int index, float newValue)
{
    switch (index)
	{
		case LCOnParam:
            _lc_on_param = newValue;
            break;
		case LCfreqParam:
            _lc_freq_param = newValue;
            break;
		case LCorderParam:
            _lc_order_param = newValue;
            break;

		case HCOnParam:
            _hc_on_param = newValue;
            break;

		case HCfreqParam:
            _hc_freq_param = newValue;
            break;
		case HCorderParam:
            _hc_order_param = newValue;
            break;

        case PF1GainParam:
            _pf1_gain_param = newValue;
            break;
        case PF1freqParam:
            _pf1_freq_param = newValue;
            break;
        case PF1QParam:
            _pf1_q_param = newValue;
            break;

        case PF2GainParam:
            _pf2_gain_param = newValue;
            break;
        case PF2freqParam:
            _pf2_freq_param = newValue;
            break;
        case PF2QParam:
            _pf2_q_param = newValue;
            break;

    case LSGainParam:
      _ls_gain_param = newValue;
      break;
    case LSfreqParam:
      _ls_freq_param = newValue;
      break;
    case LSQParam:
      _ls_q_param = newValue;
      break;

    case HSGainParam:
      _hs_gain_param = newValue;
      break;
    case HSfreqParam:
      _hs_freq_param = newValue;
      break;
    case HSQParam:
      _hs_q_param = newValue;
      break;


		default:
            break;
	}

    updateFilterParameters(getSampleRate(), true);

    sendChangeMessage();
}

const String LowhighpassAudioProcessor::getParameterName (int index)
{
    switch (index)
	{
		case LCOnParam:    return "LowCut On";
		case LCfreqParam:    return "LowCut Freq";
		case LCorderParam:    return "LowCut Order";

		case HCOnParam:    return "HighCut On";
		case HCfreqParam:    return "HighCut Freq";
		case HCorderParam:    return "HighCut Order";

        case PF1GainParam:    return "Peak 1 Gain";
        case PF1freqParam:    return "Peak 1 Freq";
        case PF1QParam:    return "Peak 1 Q";

        case PF2GainParam:    return "Peak 2 Gain";
        case PF2freqParam:    return "Peak 2 Freq";
        case PF2QParam:    return "Peak 2 Q";

        case LSGainParam:    return "LowShelf Gain";
        case LSfreqParam:    return "LowShelf Freq";
        case LSQParam:    return "LowShelf Q";

        case HSGainParam:    return "HighShelf Gain";
        case HSfreqParam:    return "HighShelf Freq";
        case HSQParam:    return "HighShelf Q";

		default:            return "";
	}
}


const String LowhighpassAudioProcessor::getParameterText (int index)
{
    String text;

    switch (index)
	{
        case LCOnParam:
            if (_lc_on_param <= 0.5f)
                text = "Off";
            else
                text = "On";
            break;

        case LCfreqParam:
            text = String(param2freq(_lc_freq_param)).substring(0, 6);
            text << " Hz";
            break;

        case LCorderParam:
            if (_lc_order_param <= 0.5f)
                text = "2nd";
            else
                text = "4th";
            break;


        case HCOnParam:
            if (_hc_on_param <= 0.5f)
                text = "Off";
            else
                text = "On";
            break;

        case HCfreqParam:
            text = String(param2freq(_hc_freq_param)).substring(0, 6);
            text << " Hz";
            break;

        case HCorderParam:
            if (_hc_order_param <= 0.5f)
                text = "2nd";
            else
                text = "4th";
            break;

        case PF1GainParam:
            text = String(param2db(_pf1_gain_param)).substring(0, 4);
            text << " dB";
            break;

        case PF1freqParam:
            text = String(param2freq(_pf1_freq_param)).substring(0, 6);
            text << " Hz";
            break;

        case PF1QParam:
            text = String(param2q(_pf1_q_param)).substring(0, 5);
            break;



        case PF2GainParam:
            text = String(param2db(_pf2_gain_param)).substring(0, 4);
            text << " dB";
            break;

        case PF2freqParam:
            text = String(param2freq(_pf2_freq_param)).substring(0, 6);
            text << " Hz";
            break;

        case PF2QParam:
            text = String(param2q(_pf2_q_param)).substring(0, 5);
            break;


    case LSGainParam:
      text = String(param2db(_ls_gain_param)).substring(0, 4);
      text << " dB";
      break;

    case LSfreqParam:
      text = String(param2freq(_ls_freq_param)).substring(0, 6);
      text << " Hz";
      break;

    case LSQParam:
      text = String(param2q(_ls_q_param)).substring(0, 5);
      break;


    case HSGainParam:
      text = String(param2db(_hs_gain_param)).substring(0, 4);
      text << " dB";
      break;

    case HSfreqParam:
      text = String(param2freq(_hs_freq_param)).substring(0, 6);
      text << " Hz";
      break;

    case HSQParam:
      text = String(param2q(_hs_q_param)).substring(0, 5);
      break;


		default:
            break;
	}

	return text;
}

const String LowhighpassAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String LowhighpassAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool LowhighpassAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool LowhighpassAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool LowhighpassAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LowhighpassAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LowhighpassAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double LowhighpassAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LowhighpassAudioProcessor::getNumPrograms()
{
    return 0;
}

int LowhighpassAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LowhighpassAudioProcessor::setCurrentProgram (int index)
{
}

const String LowhighpassAudioProcessor::getProgramName (int index)
{
    return String();
}

void LowhighpassAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void LowhighpassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    resetFilters(sampleRate);

    updateFilterParameters(sampleRate, false);
    _analysis_inbuf.clear();
    _analysis_outbuf.clear();

}

void LowhighpassAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void LowhighpassAudioProcessor::updateFilterParameters(double sampleRate, bool fade)
{
    // update filter parameters!
    _LC_IIR_1.setFilterParameters(sampleRate, param2freq(_lc_freq_param), 1.f/sqrt(2.f), 0.f, fade);
    _LC_IIR_2.setFilterParameters(sampleRate, param2freq(_lc_freq_param), 1.f/sqrt(2.f), 0.f, fade);

    _HC_IIR_1.setFilterParameters(sampleRate, param2freq(_hc_freq_param), 1.f/sqrt(2.f), 0.f, fade);
    _HC_IIR_2.setFilterParameters(sampleRate, param2freq(_hc_freq_param), 1.f/sqrt(2.f), 0.f, fade);

    _PF_IIR_1.setFilterParameters(sampleRate, param2freq(_pf1_freq_param), param2q(_pf1_q_param), param2gain(_pf1_gain_param), fade);
    _PF_IIR_2.setFilterParameters(sampleRate, param2freq(_pf2_freq_param), param2q(_pf2_q_param), param2gain(_pf2_gain_param), fade);

    _LS_IIR.setFilterParameters(sampleRate, param2freq(_ls_freq_param), param2q(_ls_q_param), param2gain(_ls_gain_param), fade);

    _HS_IIR.setFilterParameters(sampleRate, param2freq(_hs_freq_param), param2q(_hs_q_param), param2gain(_hs_gain_param), fade);
}

void LowhighpassAudioProcessor::resetFilters(double sampleRate)
{
    float const interpolationTimeInSeconds = 0.05;

    _LC_IIR_1.reset();
    _LC_IIR_1.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _LC_IIR_2.reset();
    _LC_IIR_2.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _HC_IIR_1.reset();
    _HC_IIR_1.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _HC_IIR_2.reset();
    _HC_IIR_2.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _PF_IIR_1.reset();
    _PF_IIR_1.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _PF_IIR_2.reset();
    _PF_IIR_2.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _LS_IIR.reset();
    _LS_IIR.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

    _HS_IIR.reset();
    _HS_IIR.setInterpolationTime(sampleRate, interpolationTimeInSeconds);

}

freqResponse LowhighpassAudioProcessor::getResponse(double f)
{
    std::complex<double> const wn (0, 2.f*M_PI*f / getSampleRate());

    std::complex<double> const z = pow(M_E, wn);

    std::complex <double> Y (0.f, 0.f);
    std::complex <double> X (0.f, 0.f);

    std::complex<double> compl_zero (0.f, 0.f); // use this for OS independent reset

    std::complex <double> H (1,0);
    ///////////////////
    // get the freq response of the low cut
    if (_lc_on_param > 0.5f) {

        Y=compl_zero;
        X=compl_zero;

        Y += (double)_LC_IIR_1.getB0(); // b0
        Y += (double)_LC_IIR_1.getB1() / pow(z, 1); // b1
        Y += (double)_LC_IIR_1.getB2() / pow(z, 2); // b2

        X += 1.0; // a0 = 1, coeffs are normalized
        X += (double)_LC_IIR_1.getA1() / pow(z, 1); // a1
        X += (double)_LC_IIR_1.getA2() / pow(z, 2); // a2

        std::complex <double> H_temp = Y / X;
        H *= H_temp;

        if (_lc_order_param > 0.5f)
        {
            H *= H_temp;
        }
    }
    ///////////////////
    // get the freq response of the high cut
    if (_hc_on_param > 0.5f) {

        Y=compl_zero;
        X=compl_zero;

        Y += (double)_HC_IIR_1.getB0(); // b0
        Y += (double)_HC_IIR_1.getB1() / pow(z, 1); // b1
        Y += (double)_HC_IIR_1.getB2() / pow(z, 2); // b2

        X += 1.0;
        X += (double)_HC_IIR_1.getA1() / pow(z, 1); // a1
        X += (double)_HC_IIR_1.getA2() / pow(z, 2); // a2

        std::complex <double> H_temp = Y / X;
        H *= H_temp;

        if (_hc_order_param > 0.5f)
        {
            H *= H_temp;
        }
    }

    ///////////////////
    // get freq response of peak 1

    Y=compl_zero;
    X=compl_zero;

    Y += (double)_PF_IIR_1.getB0(); // b0
    Y += (double)_PF_IIR_1.getB1() / pow(z, 1); // b1
    Y += (double)_PF_IIR_1.getB2() / pow(z, 2); // b2

    X += 1.0;
    X += (double)_PF_IIR_1.getA1() / pow(z, 1); // a1
    X += (double)_PF_IIR_1.getA2() / pow(z, 2); // a2

    H *= Y / X;

    ///////////////////
    // get freq response of peak 2

    Y=compl_zero;
    X=compl_zero;

    Y += (double)_PF_IIR_2.getB0(); // b0
    Y += (double)_PF_IIR_2.getB1() / pow(z, 1); // b1
    Y += (double)_PF_IIR_2.getB2() / pow(z, 2); // b2

    X += 1.0;
    X += (double)_PF_IIR_2.getA1() / pow(z, 1); // a1
    X += (double)_PF_IIR_2.getA2() / pow(z, 2); // a2

    H *= Y / X;

    ///////////////////
    // get freq response of low shelf

    Y=compl_zero;
    X=compl_zero;

    Y += (double)_LS_IIR.getB0(); // b0
    Y += (double)_LS_IIR.getB1() / pow(z, 1); // b1
    Y += (double)_LS_IIR.getB2() / pow(z, 2); // b2

    X += 1.0;
    X += (double)_LS_IIR.getA1() / pow(z, 1); // a1
    X += (double)_LS_IIR.getA2() / pow(z, 2); // a2

    H *= Y / X;


    ///////////////////
    // get freq response of high shelf

    Y=compl_zero;
    X=compl_zero;

    Y += (double)_HS_IIR.getB0(); // b0
    Y += (double)_HS_IIR.getB1() / pow(z, 1); // b1
    Y += (double)_HS_IIR.getB2() / pow(z, 2); // b2

    X += 1.0;
    X += (double)_HS_IIR.getA1() / pow(z, 1); // a1
    X += (double)_HS_IIR.getA2() / pow(z, 2); // a2

    H *= Y / X;

    freqResponse response;

    response.magnitude = abs(H);

    response.phase = arg(H);

    return response;
}

float LowhighpassAudioProcessor::inMagnitude(double f)
{
    return _in_mag[(int)floor( (f / getSampleRate()*(double)FFT_LENGTH) + 0.5f)];
}

float LowhighpassAudioProcessor::outMagnitude(double f)
{
    return _out_mag[(int)floor( (f / getSampleRate()*(double)FFT_LENGTH)  + 0.5f)];
}

void LowhighpassAudioProcessor::freqanalysis(bool activate)
{
    _freqanalysis = activate;
}

bool LowhighpassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return ((layouts.getMainOutputChannelSet().size() == NUM_CHANNELS) &&
            (layouts.getMainInputChannelSet().size() == NUM_CHANNELS));
}

void LowhighpassAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    int const numSamples = buffer.getNumSamples();

    int const numChannels = buffer.getNumChannels();

    // this is for the fft analysis
    int x1 = numSamples;
    int x2 = 0;

    // if we want to analyse the signal sum all channels
    if (_freqanalysis && _editorOpen)
    {
        AudioSampleBuffer tempBuffer(1,numSamples);
        tempBuffer.clear();
        for (int i=0; i<numChannels; i++) {
            tempBuffer.addFrom(0, 0, buffer, i, 0, numSamples);
        }

        if (_bufpos+x1 > FFT_LENGTH)
        {
            x1 = FFT_LENGTH - _bufpos;
            x2 = numSamples - x1;
        }

        _analysis_inbuf.copyFrom(0, _bufpos, tempBuffer, 0, 0, x1);

        if (x2 > 0)
        {
            _analysis_inbuf.copyFrom(0, 0, tempBuffer, 0, x1, x2);
        }

    }



    // LOW CUT - bypass is not clickfree
    if (_lc_on_param > 0.5f) {
        _LC_IIR_1.processBlock(buffer);

        // second stage -> 4th order butterworth
        if (_lc_order_param > 0.5f)
            _LC_IIR_2.processBlock(buffer);
    }


    // HIGH CUT - bypass is not clickfree
    if (_hc_on_param > 0.5f) {
        _HC_IIR_1.processBlock(buffer);

        // second stage -> 4th order butterworth
        if (_hc_order_param > 0.5f)
            _HC_IIR_2.processBlock(buffer);
    }

    _PF_IIR_1.processBlock(buffer);
    _PF_IIR_2.processBlock(buffer);

    _LS_IIR.processBlock(buffer);
    _HS_IIR.processBlock(buffer);


    // if we want to analyse the signal sum all channels
    if (_freqanalysis && _editorOpen)
    {
        AudioSampleBuffer tempBuffer(1,numSamples);
        tempBuffer.clear();
        for (int i=0; i<numChannels; i++) {
            tempBuffer.addFrom(0, 0, buffer, i, 0, numSamples);
        }

        if (_bufpos+x1 > FFT_LENGTH)
        {
            x1 = FFT_LENGTH - _bufpos;
            x2 = numSamples - x1;
        }

        _analysis_outbuf.copyFrom(0, _bufpos, tempBuffer, 0, 0, x1);

        if (x2 > 0)
        {
            _analysis_outbuf.copyFrom(0, 0, tempBuffer, 0, x1, x2);
        }

        _bufpos += numSamples;

        if (_bufpos >= FFT_LENGTH)
        {
            // do fft if buffer is full

            // first the input signal
            FloatVectorOperations::copy(fft_t_, _analysis_inbuf.getReadPointer(0), FFT_LENGTH);

            // windowing
            FloatVectorOperations::multiply(fft_t_, _w, FFT_LENGTH);

            // do "correct" scaling
            // FloatVectorOperations::multiply(fft_t_, 1.f/FFT_LENGTH, FFT_LENGTH);

#if SPLIT_COMPLEX

            vDSP_ctoz(reinterpret_cast<const COMPLEX*>(fft_t_), 2, &splitcomplex_, 1, FFT_LENGTH/2);
            vDSP_fft_zrip(vdsp_fft_setup_, &splitcomplex_, 1, vdsp_log2_, FFT_FORWARD);

            fft_re_[FFT_LENGTH/2] = fft_im_[0];
            fft_im_[0] = 0.f;
            fft_im_[FFT_LENGTH/2] = 0.f;

            // get magnitude
            for (int i=0; i<FFT_LENGTH/2+1; i++) {
                _in_mag[i] = 0.8f * _in_mag[i] + 0.2f * (sqrtf(fft_re_[i]*fft_re_[i]+fft_im_[i]*fft_im_[i]));
            }

#else
            if (fftwf_plan_r2c_)
                fftwf_execute_dft_r2c (fftwf_plan_r2c_, fft_t_, fft_c_);

            // get magnitude
            for (int i=0; i<FFT_LENGTH/2+1; i++) {
                _in_mag[i] = 0.8f * _in_mag[i] + 0.2f * (sqrtf(fft_c_[i][0]*fft_c_[i][0]+fft_c_[i][1]*fft_c_[i][1]));
            }
#endif





            // scale for the maximum to be 0 dB

            float max = FloatVectorOperations::findMaximum(_in_mag, FFT_LENGTH/2+1);
            float scale = (max == 0.f) ? 1.f : 1.f / max;

            FloatVectorOperations::multiply(_in_mag, scale, FFT_LENGTH/2+1);


            // do the output

            FloatVectorOperations::copy(fft_t_, _analysis_outbuf.getReadPointer(0), FFT_LENGTH);

            // windowing
            FloatVectorOperations::multiply(fft_t_, _w, FFT_LENGTH);

            // do scaling
            // FloatVectorOperations::multiply(fft_t_, 1.f/FFT_LENGTH, FFT_LENGTH);

            // FloatVectorOperations::clear(&fft_c_[0][0], 2*(FFT_LENGTH/2+1));

#if SPLIT_COMPLEX

            vDSP_ctoz(reinterpret_cast<const COMPLEX*>(fft_t_), 2, &splitcomplex_, 1, FFT_LENGTH/2);
            vDSP_fft_zrip(vdsp_fft_setup_, &splitcomplex_, 1, vdsp_log2_, FFT_FORWARD);

            fft_re_[FFT_LENGTH/2] = fft_im_[0];
            fft_im_[0] = 0.f;
            fft_im_[FFT_LENGTH/2] = 0.f;

            // get magnitude
            for (int i=0; i<FFT_LENGTH/2+1; i++) {
                _out_mag[i] = 0.8f * _out_mag[i] + 0.2f * (sqrtf(fft_re_[i]*fft_re_[i]+fft_im_[i]*fft_im_[i]));
            }

#else
            fftwf_execute_dft_r2c (fftwf_plan_r2c_, fft_t_, fft_c_);

            // get magnitude
            for (int i=0; i<FFT_LENGTH/2+1; i++) {
                _out_mag[i] = 0.8f * _out_mag[i] + 0.2f * (sqrtf(fft_c_[i][0]*fft_c_[i][0]+fft_c_[i][1]*fft_c_[i][1]));
            }
#endif

            FloatVectorOperations::multiply(_out_mag, scale, FFT_LENGTH/2+1);


            _bufpos -= FFT_LENGTH;
        }

    }

}

//==============================================================================
bool LowhighpassAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}


AudioProcessorEditor* LowhighpassAudioProcessor::createEditor()
{
    return new LowhighpassAudioProcessorEditor (this);
}

//==============================================================================
void LowhighpassAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // Create an outer XML element..

    XmlElement xml ("MYPLUGINSETTINGS");

    // add some attributes to it..
    for (int i=0; i < getNumParameters(); i++)
    {
        xml.setAttribute (String(i), getParameter(i));
    }

    xml.setAttribute("freqanalysis", _freqanalysis);

    // then use this helper function to stuff it into the binary blob and return it..
    copyXmlToBinary (xml, destData);
}

void LowhighpassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            for (int i=0; i < getNumParameters(); i++) {
                setParameter(i, xmlState->getDoubleAttribute(String(i)));
            }
            _freqanalysis = xmlState->getBoolAttribute("freqanalysis");
        }

    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowhighpassAudioProcessor();
}
