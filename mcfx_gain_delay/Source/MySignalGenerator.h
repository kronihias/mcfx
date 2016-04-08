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

#ifndef __mcfx_gain_delay__MySignalGenerator__
#define __mcfx_gain_delay__MySignalGenerator__

#include <JuceHeader.h>

#define SIGTABLEN 512

/////////////////////////////////

/* RANDOM GENERATOR from Numerical Recipes in C */

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

class MyRandomGenerator
{
public:
  MyRandomGenerator()
  {
    iy = 0;
    setSeed(-123456789L);
    
    iset=0;
  }
  
  ~MyRandomGenerator()
  {
    
  }
  
  
  /* initialize first with negative integer (long) */
  void setSeed(long idum_)
  {
    idum = idum_;
    
    int j;
    long k;
    
    if (idum <= 0 || !iy) {
      if (-(idum) < 1)
        idum=1;
      else
        idum = -(idum);
      for (j=NTAB+7;j>=0;j--)
      {
        k=idum/IQ;
        idum=IA*(idum-k*IQ)-IR*k;
        if (idum < 0) idum += IM;
        if (j < NTAB) iv[j] = idum;
      }
      
      iy=iv[0];
    }
  }
  
  /* Random number between -1.0 and 1.0 */
  float getRandomNumber()
  {
    float temp;
    
    int j;
    
    long k=idum/IQ;
    idum=IA*(idum-k*IQ)-IR*k;
    
    if (idum < 0) idum += IM;
    
    j=iy/NDIV;
    iy=iv[j];
    iv[j] = idum;
    
    temp=AM*iy;
    
    if (temp > RNMX)
      return (2.f * RNMX - 1.f);
    else
      return (2.f * temp - 1.f);
    
  }
  
  /* Get Gaussian distributed Random Number */
  /* zero mean and unit variance */
  float getGaussianRandomNumber()
  {
    
    float fac,rsq,v1,v2;
    
    if (iset == 0) {
      
      do {
        
        v1=getRandomNumber();
        v2=getRandomNumber();
        rsq=v1*v1+v2*v2;
        
      } while (rsq >= 1.0 || rsq == 0.0);
      
      fac=sqrt(-2.0*log(rsq)/rsq);
      gset=v1*fac;
      
      return v2*fac*0.2239f; // -12dB
    }
    else
    {
      iset=0;
      return gset;
    }
  }
  
  /* Fill all channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithGaussianRandomNumbers(AudioSampleBuffer &buffer)
  {
    int numSamples = (&buffer)->getNumSamples();
    int numChannels = (&buffer)->getNumChannels();
    
    for (int i=0; i<numChannels; i++) {
      float* wp = (&buffer)->getWritePointer(i);
      
      for (int k=0; k<numSamples; k++) {
        (*wp++) = getGaussianRandomNumber();
      }
    }
    
  }
  
  /* Fill one channel in AudioBuffer with random numbers aka white noise */
  void fillBufferWithGaussianRandomNumbers(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    
    for (int k=0; k<numSamples; k++) {
      (*wp++) = getGaussianRandomNumber();
    }
    
  }
  
  /* Fill all channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithRandomNumbers(AudioSampleBuffer &buffer)
  {
    int numSamples = (&buffer)->getNumSamples();
    int numChannels = (&buffer)->getNumChannels();
    
    for (int i=0; i<numChannels; i++) {
      float* wp = (&buffer)->getWritePointer(i);
      
      for (int k=0; k<numSamples; k++) {
        (*wp++) = getRandomNumber();
      }
    }
    
  }
  
  /* Fill one channels in AudioBuffer with random numbers aka white noise */
  void fillBufferWithRandomNumbers(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    
    for (int k=0; k<numSamples; k++) {
      (*wp++) = getRandomNumber();
    }
    
  }
  
private:
  
  long idum;
  long iy;
  long iv[NTAB];
  
  
  // for gaussian
  float gset;
  bool iset;
};


/* Pink Noise Filter */

class MyPinkNoiseFilter
{
public:
  MyPinkNoiseFilter(int numchannels=1)
  {
    numChannels_ = numchannels;
    
    b0_.resize(numChannels_);
    b1_.resize(numChannels_);
    b2_.resize(numChannels_);
    b3_.resize(numChannels_);
    b4_.resize(numChannels_);
    b5_.resize(numChannels_);
    b6_.resize(numChannels_);
    
    for (int i=0; i < numChannels_; i++)
    {
      b0_.set(i, 0.f);
      b1_.set(i, 0.f);
      b2_.set(i, 0.f);
      b3_.set(i, 0.f);
      b4_.set(i, 0.f);
      b5_.set(i, 0.f);
      b6_.set(i, 0.f);
    }
  }
  
  ~MyPinkNoiseFilter()
  {
    
  }
  
  // http://www.firstpr.com.au/dsp/pink-noise/
  void FilterAudioBuffer(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = (&buffer)->getNumSamples();
    
    float* wp = (&buffer)->getWritePointer(ch);
    float* b0 = &b0_.getReference(ch);
    float* b1 = &b1_.getReference(ch);
    float* b2 = &b2_.getReference(ch);
    float* b3 = &b3_.getReference(ch);
    float* b4 = &b4_.getReference(ch);
    float* b5 = &b5_.getReference(ch);
    float* b6 = &b6_.getReference(ch);
      
      for (int k=0; k<numSamples; k++) {
        
        float white = *wp;
        
        *b0 = 0.99886 * *b0 + white * 0.0555179;
        *b1 = 0.99332 * *b1 + white * 0.0750759;
        *b2 = 0.96900 * *b2 + white * 0.1538520;
        *b3 = 0.86650 * *b3 + white * 0.3104856;
        *b4 = 0.55000 * *b4 + white * 0.5329522;
        *b5 = -0.7616 * *b5 - white * 0.0168980;
        (*wp++) = (*b0 + *b1 + *b2 + *b3 + *b4 + *b5 + *b6 + white * 0.5362) * 0.2959f;
        *b6 = white * 0.115926;
        
      }

  }
  
  void FilterAudioBuffer(AudioSampleBuffer &buffer)
  {
    int numChannels = jmin((&buffer)->getNumChannels(), numChannels_);
    
    for (int i=0; i<numChannels; i++) {
      FilterAudioBuffer(buffer, i);
    }
  }
  
private:
  Array<float> b0_, b1_, b2_, b3_, b4_, b5_, b6_;
  int numChannels_;
  
};

/* PROTOTYPE TONE GENERATOR */

class MyToneGenerator
{
public:
  MyToneGenerator() : sigtable_(1,256), phase_(0.), dphase_(0.)
  {
    f_ = 440.f;
    samplerate_ = 44100.f;
    
    // updateTable();
  }
  
  virtual ~MyToneGenerator() {};
  
  void setSamplerate(float samplerate)
  {
    if (samplerate <= 0)
      samplerate = 44100.f;
    
    samplerate_ = samplerate;
    
  }
  
  void setFreq(float f)
  {
    if (f <= 0)
      f = 440.f;
    
    f_ = f;
    
    dphase_ = SIGTABLEN * f_ / samplerate_;
  }
  
  float getFreq()
  {
    return f_;
  }
  
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch)
  {
    int numSamples = buffer.getNumSamples();
    
    float *out = buffer.getWritePointer(ch);
    float *tab = sigtable_.getWritePointer(0);
    
    for (int i=0; i<numSamples; i++)
    {
      float phase = floor(phase_);
      
      float frac = (float)phase_ - phase;
      
      float *addr = tab + (int)phase;
      
      float f1 = addr[0];
      float f2 = addr[1];
      
      *out++ = f1 + frac * (f2 - f1); // linear interpolation
      
      phase_ += dphase_;
      
      if (phase_ >= (double)SIGTABLEN)
        phase_ -= (double)SIGTABLEN;
    }
  }
  
protected:
  
  virtual void updateTable() = 0; // use this method to write the lookup table
  
  float samplerate_;
  float f_;
  
  AudioSampleBuffer sigtable_;
  
  double phase_; // current phase
  double dphase_; // phase increment
  
};


/* Sine Generator */
class MySineGenerator : public MyToneGenerator
{
public:
  MySineGenerator()
  {
    updateTable();
  }
  
  ~MySineGenerator()
  {}
  
private:
  
  void updateTable() // update the sine table
  {
    sigtable_.setSize(1, SIGTABLEN+1);
    sigtable_.clear();
    
    double phase = 0.f;
    double phaseinc = 2.*M_PI/(double)SIGTABLEN;
    
    float* wp = sigtable_.getWritePointer(0);
    
    for (int i=0; i<SIGTABLEN+1; i++) {
      (*wp++) = (float)sin(phase);
      phase += phaseinc;
    }
  }
  
  
};



/* Sawtooth Generator */
class MySawtoothGenerator : public MyToneGenerator
{
public:
  MySawtoothGenerator()
  {
    updateTable();
  }
  
  ~MySawtoothGenerator()
  {}
  
private:
  
  void updateTable() // update the table
  {
    sigtable_.setSize(1, SIGTABLEN+1);
    sigtable_.clear();
    
    float* wp = sigtable_.getWritePointer(0);
    
    double val = -1.;
    
    double inc = 2./SIGTABLEN;
    
    for (int i=0; i<SIGTABLEN; i++) {
      (*wp++) = (float)val*0.707f;
      val += inc;
    }
    
    (*wp++) = 0.;
    
  }
  
};

/* Square Generator */
class MySquareGenerator : public MyToneGenerator
{
public:
  MySquareGenerator()
  {
    updateTable();
  }
  
  ~MySquareGenerator()
  {}
  
private:
  
  void updateTable() // update the table
  {
    
    sigtable_.setSize(1, SIGTABLEN+1);
    sigtable_.clear();
    
    float* wp = sigtable_.getWritePointer(0);
    
    for (int i=0; i<SIGTABLEN/2; i++) {
      (*wp++) = 0.707f;
    }
    
    for (int i=0; i<SIGTABLEN/2; i++) {
      (*wp++) = -0.707f;
    }
    
    (*wp++) = 0.707f;
  }
  
};


class MySignalGenerator
{
public:
  
  MySignalGenerator(float samplerate=44100.f, int numchannels=1);
  ~MySignalGenerator();
  
  /* enum defs */
  enum sigtype { white, pink, sine, sawtooth, square, dirac, numSigTypeParams };
  enum sigtime { steady, pulsed, numSigTimeParams };
  
  /* member functs */
  void setSamplerate(float samplerate);
  
  void setGaindB(float db);
  float getGaindB();
  
  void setGain(float mag);
  float getGain();
  
  // normalized parameter 0.0 ... -inf dB;  1.0... +6dB
  void setGainParam(float param);
  float getGainParam();
  String getGainText();
  
  void setSignalType(sigtype signal_type);
  sigtype getSignalType();
  
  void setSignalTypeParam(float param);
  float getSignalTypeParam();
  String getSignalTypeText();
  
  void setSignalTime(sigtime signal_time);
  sigtime getSignalTime();
  
  void setSignalTimeParam(float param);
  float getSignalTimeParam();
  String getSignalTimeText();
  
  void setPulseLength(float seconds);
  float getPulseLength();
  
  void setFreq(float f);
  float getFreq();
  void setFreqParam(float param);
  float getFreqParam();
  String getFreqText();
  
  
  void fillBufferWithSignal(AudioSampleBuffer &buffer);
  void fillBufferWithSignal(AudioSampleBuffer &buffer, int ch);
  
  
  
  
  
private:
  int numchannels_;
  MyRandomGenerator random_gen_;
  MyPinkNoiseFilter pink_noise_filt_;
  MySineGenerator sine_generator_;
  MySawtoothGenerator sawtooth_generator_;
  MySquareGenerator square_generator_;
  
  sigtype signal_type_;
  sigtime signal_time_;
  
  float wantedgain_; // the gain requested
  
  float gainstate_; // current gain state for ramping
  
  float pulse_len_s_; // pulse length in seconds
  int pulse_len_; // length of the pulse/pause in samples
  int cur_time_smpl_; // used for pulsed signal type
  
  float samplerate_;
  
};

#endif /* defined(__mcfx_gain_delay__MySignalGenerator__) */
