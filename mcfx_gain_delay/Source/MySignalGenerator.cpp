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

#include "MySignalGenerator.h"

inline float LOG2F_( float n )
{
  // log(n)/log(2) is log2.
  return logf( n ) / logf( 2 );
}

// convert param 0...1 to freq 10Hz ... 21618Hz
inline float param2freq(float param)
{
  return powf(2.f, param*11.07f + 3.33f);
}

inline float freq2param(float freq)
{
  return (LOG2F_(freq)-3.33f)/11.07f;
}

MySignalGenerator::MySignalGenerator(float samplerate, int numchannels)
{
  numchannels_ = numchannels;
  pink_noise_filt_ = MyPinkNoiseFilter(numchannels_);
  
  signal_type_ = sine; // sawtooth; // pink;
  signal_time_ = steady; // pulsed; //
  
  pulse_len_s_ = 0.3;
  cur_time_smpl_ = 0;
  
  gainstate_ = 0.f;
  
  setSamplerate(samplerate);
  
}
MySignalGenerator::~MySignalGenerator()
{
  
}

void MySignalGenerator::setSamplerate(float samplerate)
{
  if (samplerate <= 0.f)
    samplerate = 44100.f;
  
  samplerate_ = samplerate;
  
  sine_generator_.setSamplerate(samplerate_);
  sawtooth_generator_.setSamplerate(samplerate_);
  square_generator_.setSamplerate(samplerate_);
  tuneburst_generator_.setSamplerate(samplerate_);

  setPulseLength(0.);
}

void MySignalGenerator::setGaindB(float db)
{
  if (db <= -99.f)
    wantedgain_ = 0.f;
  else
  {
    wantedgain_ = powf(10.f, db/20);
    wantedgain_ = jlimit(0.f, 4.f, wantedgain_);
  }
}

float MySignalGenerator::getGaindB()
{
  if (wantedgain_ <= 0.f)
    return -99.f;
  else
    return 20.f*log10f(wantedgain_);
}

void MySignalGenerator::setGain(float mag)
{
  wantedgain_ = jlimit(0.f, 4.f, mag);
}

float MySignalGenerator::getGain()
{
  return wantedgain_;
}

void MySignalGenerator::setGainParam(float param)
{
  if (param <= 0.f)
  {
    setGain(0.f);
  } else {
    setGaindB(jmap(param, -99.f, 6.f));
  }
}

float MySignalGenerator::getGainParam()
{
  return jmap(getGaindB(), -99.f, 6.f, 0.f, 1.f);
}

String MySignalGenerator::getGainText()
{
  String text;
  text << String(getGaindB()).substring(0, 4) << " dB";
  return text;
}

void MySignalGenerator::setSignalType(sigtype signal_type)
{
  signal_type_ = signal_type;
}

MySignalGenerator::sigtype MySignalGenerator::getSignalType()
{
  return signal_type_;
}

void MySignalGenerator::setSignalTypeParam(float param)
{
  int sigtype = floorf(param*(float)(numSigTypeParams-1));
  sigtype = jlimit(0, numSigTypeParams-1, sigtype);
  
  setSignalType((MySignalGenerator::sigtype)sigtype);
}

float MySignalGenerator::getSignalTypeParam()
{
  return (float)getSignalType()/(float)(numSigTypeParams-1);
}

String MySignalGenerator::getSignalTypeText()
{
  switch (getSignalType()) {
    case MySignalGenerator::sigtype::white:
      return "White Noise";
      break;
      
    case MySignalGenerator::sigtype::pink:
      return "Pink Noise";
      break;
      
    case MySignalGenerator::sigtype::sine:
      return "Sine";
      break;
      
    case MySignalGenerator::sigtype::sawtooth:
      return "Sawtooth";
      break;
      
    case MySignalGenerator::sigtype::square:
      return "Square";
      break;
      
    case MySignalGenerator::sigtype::dirac:
      return "Dirac";
      break;

    case MySignalGenerator::sigtype::toneburst:
      return "Toneburst";
      break;

    default:
      return "";
      break;
  }
}

void MySignalGenerator::setSignalTime(sigtime signal_time)
{
  signal_time_ = signal_time;
}

MySignalGenerator::sigtime MySignalGenerator::getSignalTime()
{
  return signal_time_;
}

void MySignalGenerator::setSignalTimeParam(float param)
{
  int sigtime = floorf(param*(float)(numSigTimeParams-1));
  sigtime = jlimit(0, numSigTimeParams-1, sigtime);
  
  setSignalTime((MySignalGenerator::sigtime)sigtime);
}

float MySignalGenerator::getSignalTimeParam()
{
  return (float)getSignalTime()/(float)(numSigTimeParams-1);
}

String MySignalGenerator::getSignalTimeText()
{
  switch (getSignalTime()) {
    case MySignalGenerator::sigtime::steady:
      return "Steady";
      break;
      
    case MySignalGenerator::sigtime::pulsed:
      return "Pulsed";
      break;
      
    default:
      return "";
      break;
  }
}

void MySignalGenerator::setPulseLength(float seconds)
{
  if (seconds > 0)
    pulse_len_s_ = seconds;
  
  pulse_len_ = (int)floorf(pulse_len_s_*samplerate_);
}

float MySignalGenerator::getPulseLength()
{
  return pulse_len_s_;
}

void MySignalGenerator::setFreq(float f)
{
  sine_generator_.setFreq(f);
  sawtooth_generator_.setFreq(f);
  square_generator_.setFreq(f);
  tuneburst_generator_.setFreq(f/6.5);
}

float MySignalGenerator::getFreq()
{
  return sine_generator_.getFreq();
}

void MySignalGenerator::setFreqParam(float param)
{
  setFreq(param2freq(param));
}

float MySignalGenerator::getFreqParam()
{
  return freq2param(getFreq());
}

String MySignalGenerator::getFreqText()
{
  String text;
  text << String(getFreq()).upToFirstOccurrenceOf(".", false, false) << " Hz";
  return text;
}

void MySignalGenerator::fillBufferWithSignal(AudioSampleBuffer &buffer)
{
  for (int i=0; i < buffer.getNumChannels(); i++)
  {
    fillBufferWithSignal(buffer, i);
  }
}

void MySignalGenerator::fillBufferWithSignal(AudioSampleBuffer &buffer, int ch)
{
  int numSamples = buffer.getNumSamples();
  
  switch (signal_type_) {
    case white:
      random_gen_.fillBufferWithGaussianRandomNumbers(buffer, ch);
      // random_gen_.fillBufferWithRandomNumbers(buffer, ch);
      break;
      
    case pink:
      random_gen_.fillBufferWithGaussianRandomNumbers(buffer, ch);
      // random_gen_.fillBufferWithRandomNumbers(buffer, ch);
      pink_noise_filt_.FilterAudioBuffer(buffer, ch);
      break;
      
    case sine:
      sine_generator_.fillBufferWithSignal(buffer, ch);
      break;
      
    case sawtooth:
      sawtooth_generator_.fillBufferWithSignal(buffer, ch);
      break;
      
    case square:
      square_generator_.fillBufferWithSignal(buffer, ch);
      break;
      
    case dirac:
      buffer.clear(ch, 0, numSamples);
      
      if (cur_time_smpl_ > 2*pulse_len_)
      {
        buffer.setSample(ch, 0, 1.f);
        cur_time_smpl_ = 0;
      }
      
      cur_time_smpl_+= numSamples;
      break;

    case toneburst:
      tuneburst_generator_.fillBufferWithSignal(buffer, ch);
      break;
      
    default:
      break;
      
  }
  
  float tmpgain = wantedgain_;
  
  // calc gain value for pulsed signal
  if ((signal_time_ == pulsed) && (signal_type_ != dirac))
  {
    
    if (cur_time_smpl_ < pulse_len_) {
      // tmpgain = 0.f;// let the signal pass
    }
    else if (cur_time_smpl_ < 2*pulse_len_) {
      // mute the signal
      tmpgain = 0.f;
    }
    else
    {
      // reset the couter
      cur_time_smpl_ = 0;
    }
    
    cur_time_smpl_+= numSamples;
  }
  
  buffer.applyGainRamp(ch, 0, numSamples, gainstate_, tmpgain);
  
  gainstate_ = tmpgain;
}
