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

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
Mcfx_convolverAudioProcessor::Mcfx_convolverAudioProcessor() :
_min_in_ch(0),
_min_out_ch(0),
_isProcessing(false),
_configLoaded(false)

{
    _SampleRate = getSampleRate();
    _BufferSize = getBlockSize();
    
    presetDir = presetDir.getSpecialLocation(File::userApplicationDataDirectory).getChildFile("mcfx/convolver_presets");
    std::cout << "Search dir:" << presetDir.getFullPathName() << std::endl;
    
	String debug;
    debug << "Search dir: " << presetDir.getFullPathName() << "\n\n";
    
    DebugPrint(debug);
    
    SearchPresets(presetDir);
    
    
    // this is for the open dialog of the gui
    lastDir = lastDir.getSpecialLocation(File::userHomeDirectory);
}

Mcfx_convolverAudioProcessor::~Mcfx_convolverAudioProcessor()
{
}

//==============================================================================
const String Mcfx_convolverAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int Mcfx_convolverAudioProcessor::getNumParameters()
{
    return 0;
}

float Mcfx_convolverAudioProcessor::getParameter (int index)
{
    return 0.0f;
}

void Mcfx_convolverAudioProcessor::setParameter (int index, float newValue)
{
}

const String Mcfx_convolverAudioProcessor::getParameterName (int index)
{
    return String::empty;
}

const String Mcfx_convolverAudioProcessor::getParameterText (int index)
{
    return String::empty;
}

const String Mcfx_convolverAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String Mcfx_convolverAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool Mcfx_convolverAudioProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_convolverAudioProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool Mcfx_convolverAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_convolverAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Mcfx_convolverAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double Mcfx_convolverAudioProcessor::getTailLengthSeconds() const
{
    if (_configLoaded)
    {
        return _my_convolvers.getFirst()->irLength();
    } else {
        return 0.0;
    }
}

int Mcfx_convolverAudioProcessor::getNumPrograms()
{
    return 0;
}

int Mcfx_convolverAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Mcfx_convolverAudioProcessor::setCurrentProgram (int index)
{
}

const String Mcfx_convolverAudioProcessor::getProgramName (int index)
{
    return String::empty;
}

void Mcfx_convolverAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void Mcfx_convolverAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    _SampleRate = sampleRate;
    _BufferSize = samplesPerBlock;
    
}

void Mcfx_convolverAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Mcfx_convolverAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    
    // std::cout << "in: " << getNumInputChannels() << " out: " << getNumOutputChannels() << std::endl;
    
    if (_configLoaded)
    {
        _isProcessing = true;
        
        // only process if we have enough intput/output channels
        if (getNumInputChannels() >= _min_in_ch && getNumOutputChannels() >= _min_out_ch)
        {
            
            // this buffer will hold the output
            
            AudioSampleBuffer outputBuffer(getNumOutputChannels(), buffer.getNumSamples());
            
            outputBuffer.clear();
            
            
            // iterate through all convolvers
            for (int i = 0; i < _my_convolvers.size(); i++)
            {
                
                /* inputBuffer, outputBuffer, inputChannel, outputChannel */
                
                _my_convolvers.getUnchecked(i)->process(buffer, outputBuffer, _conv_in.getUnchecked(i), _conv_out.getUnchecked(i));
                
            }
            
            buffer = outputBuffer;
        }
        
        _isProcessing = false;
        
    } else { // config loaded
        
        // clear output in case no config is loaded!
        buffer.clear();
    }
    

}

void Mcfx_convolverAudioProcessor::LoadConfiguration(File configFile)
{
    if (!configFile.existsAsFile())
    {
        
        String debug;
        debug << "Configuration file does not exist!" << configFile.getFullPathName() << "\n\n";
        
        //std::cout << debug << std::endl;
        DebugPrint(debug);
        
        return;
    }
    
    // unload first....
    if (_configLoaded) {
        
        while (_isProcessing) {
            usleep(1);
        }
        
        std::cout << "Unloading Config..." << std::endl;
        UnloadConfiguration();
        _DebugText = String(); // clear debug window
        std::cout << "Config Unloaded..." << std::endl;
    }
    
    String debug;
    debug << "\ntrying to load " << configFile.getFullPathName() << "\n\n";
    
    DebugPrint(debug);
    
    // debug print samplerate and buffer size
    debug = "Samplerate: ";
    debug << _SampleRate;
    debug << " Buffer Size: ";
    debug << _BufferSize;
    DebugPrint(debug);
    
    activePreset = configFile.getFileName(); // store filename only, on restart search preset folder for it!
    box_preset_str = configFile.getFileNameWithoutExtension();
    
    StringArray myLines;
    
    configFile.readLines(myLines);
    
    // global settings
    
    String directory("");
    
    // iterate over all lines
    for (int currentLine = 0; currentLine < myLines.size(); currentLine++)
    {
        // get the line and remove spaces from start and end
        String line (myLines[currentLine].trim());
        
        if (line.contains("/cd")) {
            
            line = line.trimCharactersAtStart("/cd").trim();
            directory = line;
            
            std::cout << "Dir: " << directory << std::endl;
            
        } else if (line.contains("/impulse/read"))
        {
            int in_ch = 0;
            int out_ch = 0;
            float gain = 1.f;
            int delay = 0;
            int offset = 0;
            int length = 0;
            int channel = 0;
            char filename[100];
            
            line = line.trimCharactersAtStart("/impulse/read").trim();
            
            String::CharPointerType lineChar = line.getCharPointer();
            
            
            sscanf(lineChar, "%i%i%f%i%i%i%i%s", &in_ch, &out_ch, &gain, &delay, &offset, &length, &channel, filename);
            
            // printf("load ir: %i %i %f %i %i %i %i %s \n", in_ch, out_ch, gain, delay, offset, length, channel, filename);
            
            _my_convolvers.add(new MyConvolver);
            
            File IrFilename;
            
            
            // check if /cd is defined in config
            if (directory.isEmpty()) {
                IrFilename = configFile.getParentDirectory().getChildFile(String(filename));
                
            } else { // /cd is defined
                if (directory.startsWith("/"))
                {
                    // absolute path is defined
                    File path(directory);
                    
                    IrFilename = path.getChildFile(String(filename));
                } else {
                    
                    // relative path to the config file is defined
                    IrFilename = configFile.getParentDirectory().getChildFile(directory).getChildFile(String(filename));
                }
            }
            
            // std::cout << IrFilename.getFullPathName() << std::endl;
            
            if (_my_convolvers.getLast()->loadIr(IrFilename, channel-1, _SampleRate, _BufferSize, gain, delay, offset, length))
            {
                _conv_in.add(in_ch-1);
                _conv_out.add(out_ch-1);
                
                
                // these values are to make sure we afterwards have enough in/out channels
                if (in_ch > _min_in_ch)
                    _min_in_ch = in_ch;
                
                if (out_ch > _min_out_ch)
                    _min_out_ch = out_ch;
                
                String debug;
                debug << "conv # " << _my_convolvers.size() << " " << IrFilename.getFullPathName() << " loaded";
                DebugPrint(debug << "\n");
                
            } else {
                
                _my_convolvers.removeLast();
                
                String debug;
                debug << "ERROR: not loaded: " << IrFilename.getFullPathName();
                DebugPrint(debug << "\n");
            }
            
        }
        
        
    } // iterate over lines
    
    _configLoaded = true;
    
    sendChangeMessage(); // notify editor
}

void Mcfx_convolverAudioProcessor::UnloadConfiguration()
{
    // delete configuration
    _configLoaded = false;
    
    for (int i=0; i < _my_convolvers.size(); i++) {
        
        _my_convolvers.getUnchecked(i)->unloadIr();
    }
    
    _my_convolvers.clear();
    _conv_in.clear();
    _conv_out.clear();
    
    _min_in_ch = 0;
    _min_out_ch = 0;
    
    std::cout << "Unloaded Convolution..." << std::endl;
    
}

void Mcfx_convolverAudioProcessor::DebugPrint(String debugText)
{
    String temp;
    
    temp << debugText;
    temp << _DebugText;
    
    // std::cout << debugText << std::endl;
    
    _DebugText = temp;
}

void Mcfx_convolverAudioProcessor::SearchPresets(File SearchFolder)
{
    _presetFiles.clear();
    
    SearchFolder.findChildFiles(_presetFiles, File::findFiles, true, "*.conf");
    std::cout << "Found preset files: " << _presetFiles.size() << std::endl;
    
}

void Mcfx_convolverAudioProcessor::LoadPreset(unsigned int preset)
{
    if (preset < (unsigned int)_presetFiles.size())
    {
        // ScheduleConfiguration(_presetFiles.getUnchecked(preset));
        LoadConfiguration(_presetFiles.getUnchecked(preset));
    }
}

void Mcfx_convolverAudioProcessor::LoadPresetByName(String presetName)
{
    Array <File> files;
    presetDir.findChildFiles(files, File::findFiles, true, presetName);
    
    if (files.size())
    {
        LoadConfiguration(files.getUnchecked(0)); // Load first result
        box_preset_str = files.getUnchecked(0).getFileNameWithoutExtension();
    }
    
}

//==============================================================================
bool Mcfx_convolverAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* Mcfx_convolverAudioProcessor::createEditor()
{
    return new Mcfx_convolverAudioProcessorEditor (this);
}

//==============================================================================
void Mcfx_convolverAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    // Create an outer XML element..
    XmlElement xml ("MYPLUGINSETTINGS");
    
    // add some attributes to it..
    xml.setAttribute ("activePreset", activePreset);
    xml.setAttribute ("presetDir", presetDir.getFullPathName());
    
    // then use this helper function to stuff it into the binary blob and return it..
    copyXmlToBinary (xml, destData);
}

void Mcfx_convolverAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        String newPresetDir;
        
        // make sure that it's actually our type of XML object..
        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
        {
            // ok, now pull out our parameters..
            activePreset  = xmlState->getStringAttribute("activePreset", "");
            
            newPresetDir = xmlState->getStringAttribute("presetDir", presetDir.getFullPathName());
        }
        
        if (activePreset.isNotEmpty()) {
            LoadPresetByName(activePreset);
        }
        
        File tempDir(newPresetDir);
        if (tempDir.exists()) {
            presetDir = tempDir;
            SearchPresets(presetDir);
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Mcfx_convolverAudioProcessor();
}
