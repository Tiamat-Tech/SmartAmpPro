/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SmartAmpProAudioProcessor::SmartAmpProAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", AudioChannelSet::stereo(), true)
#endif
    )

#endif
{

    loader.load_json("C:/Users/rache/Desktop/dev/SmartAmpPro/models/nol_small_120.json");

    LSTM.setParams(loader.hidden_size, loader.conv1d_kernel_size, loader.conv1d_1_kernel_size,
        loader.conv1d_num_channels, loader.conv1d_1_num_channels, loader.conv1d_bias_nc,
        loader.conv1d_1_bias_nc, loader.conv1d_kernel_nc, loader.conv1d_1_kernel_nc,
        loader.lstm_bias_nc, loader.lstm_kernel_nc,
        loader.dense_bias_nc, loader.dense_kernel_nc, loader.input_size_loader);
}

SmartAmpProAudioProcessor::~SmartAmpProAudioProcessor()
{
}

//==============================================================================
const String SmartAmpProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SmartAmpProAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SmartAmpProAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SmartAmpProAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SmartAmpProAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SmartAmpProAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SmartAmpProAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SmartAmpProAudioProcessor::setCurrentProgram (int index)
{
}

const String SmartAmpProAudioProcessor::getProgramName (int index)
{
    return {};
}

void SmartAmpProAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void SmartAmpProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void SmartAmpProAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SmartAmpProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

std::vector<std::vector <float>> SmartAmpProAudioProcessor::set_data(const float **inputData, int numSamples, int input_size)
{

    const float *chData = inputData[0];


    // Move input_size-1 of last buffer to the beginning of new_buffer
    for (int k = 0; k < input_size - 1; k++)
    {
        new_buffer[k] = old_buffer[numSamples + k]; // TODO double check indexing
    }

    // Update new_buffer with current buffer data
    for (int i = 0; i < numSamples; i++)
    {
        new_buffer[i + input_size - 1] = chData[i]; // TODO double check indexing
    }

    for (int i = 0; i < numSamples; i++)
    {
        for (int j = 0; j < input_size; j++) {
            //data[i][j] = chData[i + j];
            data[i][j] = new_buffer[i + j];
        }
    }

    // Build a vector of sample ranges (size of input_size) to use for LSTM input
    //for (int i = 0; i < numSamples; i++)
    //{
    //    for (int j = 0; j < input_size; j++) {
    //        data[i][j] = chData[i + j];
    //    }
    //}
    // Set the new_buffer data to old_buffer for the next block of audio
    old_buffer = new_buffer;
    
    return data;
}

void SmartAmpProAudioProcessor::check_buffer(int numSamples, int input_size)  //TODO this is called every block, how to call just at beginning and when buffer size changes?
{
    //This is done at plugin start up and when a new model is loaded
    if (old_buffer.size() != numSamples + input_size - 1) {
        std::vector<float> temp(numSamples + input_size - 1, 0.0);
        old_buffer = temp;
        new_buffer = temp;
        std::vector<std::vector<float>> temp3(numSamples, std::vector<float>(input_size, 0.0));  //vector<vector> 128, 120
        data = temp3;
        // Reset the input
        input = nc::zeros<float>(nc::Shape(input_size, 1));
        //Set initial conv1d layer 1 arrays
        LSTM.padded_xt = LSTM.pad(input, LSTM.conv1d_Kernel_Size, 12); //TODO ability to set stride
        LSTM.unfolded_xt.clear();
        for (int i = 0; i < LSTM.padded_xt.shape().rows / 12; i++)
        {
            LSTM.placeholder = LSTM.padded_xt(nc::Slice(i * 12, i * 12 + LSTM.conv1d_Kernel_Size), 0);
            LSTM.unfolded_xt.push_back(LSTM.placeholder);
        }
        // Set initial conv1d layer 2 array
        LSTM.unfolded_xt2.clear();
        nc::NdArray<float> placeholder_temp;
        LSTM.unfolded_xt2.push_back(placeholder_temp);
    }
}

void SmartAmpProAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    // Setup Audio Data
    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = getTotalNumInputChannels();
    float* channelData = buffer.getWritePointer(0);

    // Amp =============================================================================
    if (amp_state == 1) {
        //    EQ (Presence, Bass, Mid, Treble)
        eq4band.process(buffer, midiMessages, numSamples, numInputChannels);

        buffer.applyGain(ampDrive);

		// Apply LSTM model
        
        check_buffer(numSamples, LSTM.input_size);
        data = set_data(buffer.getArrayOfReadPointers(), numSamples, LSTM.input_size);
        for (int i = 0; i < numSamples; i++)
        {
            // Set the current sample input to LSTM
            for (int j = 0; j < LSTM.input_size; j++) {
                input[j] = data[i][j];
            }

            // Process LSTM for a single sample
            LSTM.conv1d_layer(input, LSTM.conv1d_kernel, LSTM.conv1d_bias, LSTM.conv1d_Kernel_Size, LSTM.conv1d_Num_Channels, 12, 1); // 12 is stride // TODO handle different strides
            LSTM.conv1d_layer2(LSTM.conv1d_out, LSTM.conv1d_1_kernel, LSTM.conv1d_1_bias, LSTM.conv1d_1_Kernel_Size, LSTM.conv1d_1_Num_Channels, 12, 2); // 12 is stride // TODO handle different strides
            LSTM.lstm_layer(LSTM.conv1d_1_out);
            LSTM.dense_layer(LSTM.lstm_out);

            // Write the LSTM result to the output buffer
            channelData[i] = LSTM.dense_out[0];

        }
        
        //    Master Volume 
        buffer.applyGain(ampMaster);

        //    Apply levelAdjust from model param (for adjusting quiet models)

    }
    
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
bool SmartAmpProAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* SmartAmpProAudioProcessor::createEditor()
{
    return new SmartAmpProAudioProcessorEditor (*this);
}

//==============================================================================
void SmartAmpProAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SmartAmpProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


void SmartAmpProAudioProcessor::loadDefault()
{
    this->suspendProcessing(true);

    loader.load_json("C:/Users/rache/Desktop/dev/SmartAmpPro/models/nol_small_120.json");  // TODO: Change ModelLoader to use JUCE json to read .json files    ***EDIT HERE***
    //loader.load_json(BinaryData::nol_small_120_json);
    LSTM.setParams(loader.hidden_size, loader.conv1d_kernel_size, loader.conv1d_1_kernel_size,
        loader.conv1d_num_channels, loader.conv1d_1_num_channels, loader.conv1d_bias_nc,
        loader.conv1d_1_bias_nc, loader.conv1d_kernel_nc, loader.conv1d_1_kernel_nc,
        loader.lstm_bias_nc, loader.lstm_kernel_nc,
        loader.dense_bias_nc, loader.dense_kernel_nc, loader.input_size_loader);

    this->suspendProcessing(false);
}


void SmartAmpProAudioProcessor::loadConfig(File configFile)
{
    this->suspendProcessing(true);

    String path = configFile.getFullPathName();
    char_filename = path.toUTF8();
    loader.load_json(char_filename);
    LSTM.setParams(loader.hidden_size, loader.conv1d_kernel_size, loader.conv1d_1_kernel_size,
        loader.conv1d_num_channels, loader.conv1d_1_num_channels, loader.conv1d_bias_nc,
        loader.conv1d_1_bias_nc, loader.conv1d_kernel_nc, loader.conv1d_1_kernel_nc,
        loader.lstm_bias_nc, loader.lstm_kernel_nc,
        loader.dense_bias_nc, loader.dense_kernel_nc, loader.input_size_loader);

    this->suspendProcessing(false);
}

float SmartAmpProAudioProcessor::convertLogScale(float in_value, float x_min, float x_max, float y_min, float y_max)
{
    float b = log(y_max / y_min) / (x_max - x_min);
    float a = y_max / exp(b * x_max);
    float converted_value = a * exp(b * in_value);
    return converted_value;
}

void SmartAmpProAudioProcessor::set_ampDrive(float db_ampDrive)
{
    ampDrive = decibelToLinear(db_ampDrive);
    ampGainKnobState = db_ampDrive;
}

void SmartAmpProAudioProcessor::set_ampMaster(float db_ampMaster)
{
    ampMaster = decibelToLinear(db_ampMaster);
    ampMasterKnobState = db_ampMaster;
}

void SmartAmpProAudioProcessor::set_ampEQ(float bass_slider, float mid_slider, float treble_slider, float presence_slider)
{
    eq4band.setParameters(bass_slider, mid_slider, treble_slider, presence_slider);

    ampPresenceKnobState = presence_slider;
}

float SmartAmpProAudioProcessor::decibelToLinear(float dbValue)
{
    return powf(10.0, dbValue/20.0);
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmartAmpProAudioProcessor();
}
