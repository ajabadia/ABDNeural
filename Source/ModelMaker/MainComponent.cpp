/*
  ==============================================================================

    MainComponent.cpp
    Created: 27 Jan 2026
    Description: Main UI implementation for NEURONiK Model Maker.

  ==============================================================================
*/

#include <juce_data_structures/juce_data_structures.h>
#include "MainComponent.h"
#include "Version.h"
#include "../Common/SpectralModelWriter.h"   // FASE 11.1: el escritor del dialecto
#include "Analysis/GridIndicator.h"          // 2026-09-27: el modelo puro del indicador

namespace NEURONiK::ModelMaker {

// 2026-09-27: listener de foco del anillo del indicador de rejilla — repinta
// al ENTRAR y al SALIR del foco (el foco JUCE no pinta nada por si solo, y un
// activable que no SE VE enfocado es la misma trampa que un texto clicable
// que no lo parece).
struct GridFocusRingCallback : public juce::FocusChangeListener
{
    explicit GridFocusRingCallback (MainComponent* o) : owner (o) {}

    MainComponent* owner = nullptr;

    void globalFocusChanged (juce::Component*) override
    {
        if (owner != nullptr)
            owner->focusStateChangedCallback();
    }
};

MainComponent::MainComponent()
    : loadButton("LOAD AUDIO"),
      waveBox("WAVEFORM INPUT"),
      spectralBox("SPECTRAL ANALYSIS (64 PARTIALS)"),
      layersBox("CAPAS (POR QUE SE PARTIO)"),
      pitchLabel("Pitch", "Detected Pitch (Hz):"),
      analyzeButton("ANALYZE"),
      playOriginalButton("PLAY ORIGINAL"),
      playModelButton("PLAY MODEL"),
      recordButton("REC"),
      stopButton("STOP"),
      exportButton("EXPORT MODEL"),
      menuBar(this) // Init with model
{
    formatManager.registerBasicFormats();

    // Menu
    addAndMakeVisible(menuBar);

    // Audio Init - Enable Inputs!
    // Initialize with 2 inputs, 2 outputs.
    // Note: If no input device is available, it might fallback or fail silently on inputs.
    deviceManager.initialiseWithDefaultDevices(2, 2); 
    deviceManager.addAudioCallback(this);

    // Header
    titleLabel.setText("NEURONiK MODEL MAKER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this]() { loadFile(); };

    fileNameLabel.setText("No file loaded", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(fileNameLabel);
    pitchGuardLabel.setFont (juce::Font (13.0f));
    pitchGuardLabel.setJustificationType (juce::Justification::centredLeft);
    pitchGuardLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (pitchGuardLabel);

    // Indicador de rejilla detectada (f0 + residuo en cents del ajuste LS)
    gridLabel.setFont (juce::Font (13.0f));
    gridLabel.setJustificationType (juce::Justification::centredLeft);
    gridLabel.setColour (juce::Label::textColourId, juce::Colours::cyan);
    // 2026-09-25: el indicador es CLICABLE. JUCE 8.0.12 no tiene `Label::onClick`,
    // asi que el clic se escucha desde el componente (mouseUp) — y el cursor y el
    // tooltip lo ANUNCIAN: un texto que se puede pulsar y no lo parece es una
    // trampa. El valor solo se carga si hay f0 detectada (sin material, el
    // indicador va vacio).
    gridLabel.setTooltip ("Clic o Espacio/Enter: cargar la f0 detectada en el editor de pitch");
    gridLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    gridLabel.addMouseListener (this, false);
    addAndMakeVisible (gridLabel);

    // 2026-09-27: el indicador es alcanzable y accionable CON TECLADO — el
    // mismo camino que el clic, no un atajo aparte. setWantsKeyboardFocus lo
    // mete en el ciclo de Tab (queda justo ANTES del editor de pitch, que es a
    // donde envia) y Space/Enter ACTUAN el clic (keyPressed, mas abajo), la
    // convencion de JUCE para todo lo activable. El anillo de foco lo dibuja
    // paintOverChildren mientras el label lo tenga.
    gridLabel.setWantsKeyboardFocus (true);
    gridFocusCallback = new GridFocusRingCallback (this);
    juce::Desktop::getInstance().addFocusChangeListener (gridFocusCallback);


    // Pitch UI
    addAndMakeVisible(pitchLabel);
    pitchLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(pitchEditor);
    pitchEditor.setText("0.0", juce::dontSendNotification);
    pitchEditor.setInputRestrictions(8, "0123456789.");
    pitchEditor.setJustification(juce::Justification::centred);
    pitchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.2f));
    pitchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan.withAlpha(0.3f));

    // 2026-09-25: MODO REJILLA FIJA. Declarado, no heuristico: el usuario dice
    // que la rejilla es suya, el analisis deja de seguir el pitch por ventana
    // (tambien por encima del suelo del estimador, que es lo que no se podia
    // pedir antes), el indicador lo anuncia y el modelo lo escribe.
    addAndMakeVisible(fixedGridButton);
    fixedGridButton.setTooltip("Rejilla FIJA: el f0 lo fijas tu. El analisis NO sigue el pitch por ventana "
                               "(en el modelo queda escrito gridFixed) y el detector no lo pisa.");
    fixedGridButton.setColour(juce::ToggleButton::textColourId, juce::Colours::cyan.withAlpha(0.8f));
    fixedGridButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::cyan);
    fixedGridButton.onClick = [this] { updateGridIndicator(); repaint(); };

    // 2026-09-25: OFFSETS TRANSPONIBLES (ratio delta-n/n). Declaracion de
    // SALIDA: no toca el analisis (los offsets se siguen midiendo en Hz
    // contra la rejilla), dice como debe interpretarlos el motor. Se aplica
    // al modelo ya analizado y a la vista previa para poder oirlo sin
    // re-analizar.
    addAndMakeVisible(offsetsTransposeButton);
    offsetsTransposeButton.setTooltip("Offsets TRANSPONIBLES: la inharmonicidad sigue al teclado. "
                                       "Los offsets se declaran ratios (delta-n/n) contra la rejilla y "
                                       "el motor los escala por base/f0 (en el modelo queda offsetsTranspose).");
    offsetsTransposeButton.setColour(juce::ToggleButton::textColourId, juce::Colours::cyan.withAlpha(0.8f));
    offsetsTransposeButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::cyan);
    offsetsTransposeButton.onClick = [this]
    {
        currentModel.offsetsTranspose = offsetsTransposeButton.getToggleState();
        if (currentModel.isValid)
            previewResonator.loadModel(currentModel, 0);
        updateGridIndicator();
        repaint();
    };

    // Note Combos
    addAndMakeVisible(noteCombo);
    noteCombo.addItem("Auto", 1);
    noteCombo.addItem("C", 2); noteCombo.addItem("C#", 3); noteCombo.addItem("D", 4);
    noteCombo.addItem("D#", 5); noteCombo.addItem("E", 6); noteCombo.addItem("F", 7);
    noteCombo.addItem("F#", 8); noteCombo.addItem("G", 9); noteCombo.addItem("G#", 10);
    noteCombo.addItem("A", 11); noteCombo.addItem("A#", 12); noteCombo.addItem("B", 13);
    noteCombo.setSelectedId(1); // Auto

    // FASE 10.4: cuantos frames temporales produce el analisis (1 = estatico).
    addAndMakeVisible(framesCombo);
    framesCombo.addItem("1 frame (estatico)", 1);
    framesCombo.addItem("2 frames", 2);
    framesCombo.addItem("3 frames", 3);
    framesCombo.addItem("4 frames", 4);
    framesCombo.addItem("6 frames", 6);
    framesCombo.addItem("8 frames", 8);
    framesCombo.setSelectedId(1);

    noteCombo.onChange = [this] { updateFreqFromRootNote(); };

    addAndMakeVisible(octaveCombo);
    octaveCombo.addItem("Auto", 1);
    for (int i = -2; i <= 8; ++i)
        octaveCombo.addItem(juce::String(i), i + 4); // ID offset to avoid clash with Auto=1
    octaveCombo.setSelectedId(1); // Auto
    octaveCombo.onChange = [this] { updateFreqFromRootNote(); };
    
    // Visualizers
    addAndMakeVisible(waveBox);
    addAndMakeVisible(spectralBox);
    addAndMakeVisible(layersBox);

    // Footer
    addAndMakeVisible(playOriginalButton);
    playOriginalButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2ECC71).withAlpha(0.2f));
    playOriginalButton.onClick = [this]() { playOriginal(); };

    addAndMakeVisible(playModelButton);
    playModelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF9B59B6).withAlpha(0.2f));
    playModelButton.onClick = [this]() { playModel(); };

    addAndMakeVisible(recordButton);
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.6f));
    recordButton.onClick = [this]() 
    {
        if (isRecording) stopRecording();
        else startRecording();
    };

    addAndMakeVisible(stopButton);
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE74C3C).withAlpha(0.2f));
    stopButton.onClick = [this]() { stopPlayback(); };

    addAndMakeVisible(analyzeButton);
    analyzeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD35400)); // Orange accent
    analyzeButton.onClick = [this]() { analyzeAudio(); };

    addAndMakeVisible(exportButton);
    exportButton.onClick = [this]() { exportModel(); };

    // Initial state
    playOriginalButton.setEnabled(false);
    playModelButton.setEnabled(false);
    stopButton.setEnabled(false);
    exportButton.setEnabled(false);
    analyzeButton.setEnabled(false);

    juce::String savedZoom = loadSetting("zoomScale");
    if (savedZoom.isNotEmpty())
        setZoom(savedZoom.getFloatValue());
    else
        setSize(800, 600);
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(this);
    transportSource.setSource(nullptr);

    // 2026-09-27: el listener del anillo de foco se da de baja (y se libera)
    // ANTES de que los miembros empiecen a destruirse.
    if (gridFocusCallback != nullptr)
    {
        juce::Desktop::getInstance().removeFocusChangeListener (gridFocusCallback);
        delete gridFocusCallback;
        gridFocusCallback = nullptr;
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF101010)); // Deep dark background

    // Subtle background mesh or gradient
    juce::ColourGradient bgGrad(juce::Colour(0xFF1A1A1A), 0, 0,
                               juce::Colour(0xFF050505), 0, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Draw Spectral View
    auto specArea = spectralBox.getBounds().reduced(10).withTrimmedTop(25);
    paintSpectralView(g, specArea);

    // Draw Waveform View
    auto waveArea = waveBox.getBounds().reduced(10).withTrimmedTop(25);
    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.6f));
        thumbnail.drawChannels(g, waveArea, 0.0, thumbnail.getTotalLength(), 1.0f);
    }

    // Draw Layer View (FASE 11.5)
    auto layerArea = layersBox.getBounds().reduced(10).withTrimmedTop(25);
    paintLayerView(g, layerArea);
}

// --- Audio Callbacks ---

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    transportSource.prepareToPlay(512, currentSampleRate); 
    
    previewResonator.setSampleRate(currentSampleRate);
    previewResonator.reset();
}

void MainComponent::audioDeviceStopped()
{
    transportSource.releaseResources();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);

    // Recording Logic
    if (isRecording && numInputChannels > 0)
    {
        int currentSize = recordingBuffer.getNumSamples();
        if (currentSize < 10 * 60 * 48000) // Limit to 10 mins
        {
            recordingBuffer.setSize(1, currentSize + numSamples, true, true, false);
            recordingBuffer.copyFrom(0, currentSize, inputChannelData[0], numSamples);
        }
    }

    // Clear outputs
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

    // Create a proxy buffer wrapper for the external output data
    juce::AudioBuffer<float> proxyOutputBuffer(outputChannelData, numOutputChannels, numSamples);
    juce::AudioSourceChannelInfo bufferToFill(&proxyOutputBuffer, 0, numSamples);
    
    transportSource.getNextAudioBlock(bufferToFill);

    if (isPlayingModel)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // Indice dentro del bloque: la ruta de entropia lee su jitter por indice
            // (la ruta SIMD por defecto no lo necesita). Si algun dia se activa
            // entropy en el preview, hay que llamar a prepareJitterBuffers() antes.
            float samp = previewResonator.processSample(i) * 0.5f; 
            if (i < bufferToFill.numSamples)
            {
                for (int ch = 0; ch < numOutputChannels; ++ch)
                    outputChannelData[ch][i] += samp;
            }
        }
    }
}

// --- Recording & Sync ---

void MainComponent::startRecording()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    stopPlayback();

    recordingBuffer.setSize(1, 0); 
    isRecording = true;
    
    recordButton.setButtonText("STOP REC");
    playOriginalButton.setEnabled(false);
    playModelButton.setEnabled(false);
    analyzeButton.setEnabled(false);
    fileNameLabel.setText("Recording...", juce::dontSendNotification);
}

void MainComponent::stopRecording()
{
    isRecording = false;
    recordButton.setButtonText("REC");

    if (recordingBuffer.getNumSamples() > 0)
    {
        loadedAudio.makeCopyOf(recordingBuffer);
        loadedSampleRate = currentSampleRate;

        thumbnail.reset(1, loadedSampleRate, loadedAudio.getNumSamples());
        thumbnail.addBlock(0, loadedAudio, 0, loadedAudio.getNumSamples());

        // Write to a temporary file so we can reuse the loading/transport logic
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NEURONiK_Recording.wav");
        
        {
            juce::WavAudioFormat wavFormat;
            // JUCE 8: the (OutputStream*, ...) overload is deprecated — AudioFormatWriterOptions
            // instead. The stream travels in a unique_ptr: ownership moves to the writer on
            // success; on failure this scope releases it (old API deleted it internally).
            std::unique_ptr<juce::OutputStream> stream(new juce::FileOutputStream(tempFile));
            juce::AudioFormatWriterOptions options;
            options = options.withSampleRate(loadedSampleRate)
                .withNumChannels((int) loadedAudio.getNumChannels())
                .withBitsPerSample(16);
            auto writer = wavFormat.createWriterFor(stream, options);
            
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(loadedAudio, 0, loadedAudio.getNumSamples());
                writer.reset(); // Close file
            }
        }

        if (tempFile.existsAsFile())
        {
            auto* reader = formatManager.createReaderFor(tempFile);
            if (reader != nullptr)
            {
                readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
                transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
            }
        }

        playOriginalButton.setEnabled(true);
        analyzeButton.setEnabled(true);
        fileNameLabel.setText("Recorded Audio", juce::dontSendNotification);
        
        // Auto-detect pitch immediately? Or wait. Let's wait for user or automate it?
        // Plan says: "auto-detect pitch".
        detectedFrequency = analyzer.detectPitch(loadedAudio, loadedSampleRate);
        pitchEditor.setText(juce::String(detectedFrequency, 2), juce::dontSendNotification);
        updateRootNoteFromFreq(detectedFrequency);
        updateGridIndicator();
    }
}

void MainComponent::updateFreqFromRootNote()
{
    int noteId = noteCombo.getSelectedId();
    int octId = octaveCombo.getSelectedId();

    if (noteId > 1 && octId > 1) 
    {
        int semi = noteId - 2; 
        int octave = octId - 4;
        
        int midiNote = (octave + 2) * 12 + semi;
        midiNote = juce::jlimit(0, 127, midiNote);
        
        float freq = (float)juce::MidiMessage::getMidiNoteInHertz(midiNote);
        pitchEditor.setText(juce::String(freq, 2));
    }
}

void MainComponent::updateRootNoteFromFreq(float freqHz)
{
    if (freqHz <= 0) return;
    
    int midiNote = juce::roundToInt(12.0 * std::log2(freqHz / 440.0) + 69.0);
    
    int octave = (midiNote / 12) - 2; 
    int semi = midiNote % 12;
    
    int noteId = semi + 2;
    int octId = octave + 4;
    
    if (noteCombo.getSelectedId() == 1) noteCombo.setSelectedId(noteId, juce::dontSendNotification);
    if (octaveCombo.getSelectedId() == 1) octaveCombo.setSelectedId(octId, juce::dontSendNotification);
}

// --- Playback Controls ---

void MainComponent::playOriginal()
{
    stopPlayback(); // Reset state
    transportSource.setPosition(0.0);
    transportSource.start();
    stopButton.setEnabled(true);
}

void MainComponent::playModel()
{
    stopPlayback();
    previewResonator.reset();
    isPlayingModel = true;
    stopButton.setEnabled(true);
}

void MainComponent::stopPlayback()
{
    transportSource.stop();
    isPlayingModel = false;
    stopButton.setEnabled(false);
}

void MainComponent::loadFile()
{
    juce::File startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    
    juce::String lastPath = loadSetting("lastAudioPath");
    if (lastPath.isNotEmpty())
        startDir = juce::File(lastPath);

    fileChooser = std::make_unique<juce::FileChooser>("Select Audio File",
                                                     startDir,
                                                     "*.wav;*.aif;*.flac");

    auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(browserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            saveSetting("lastAudioPath", file.getParentDirectory().getFullPathName());

            auto* reader = formatManager.createReaderFor(file);
            if (reader != nullptr)
            {
                std::unique_ptr<juce::AudioFormatReader> readerPtr(reader);
                
                // Load into memory for analysis
                loadedAudio.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&loadedAudio, 0, (int)reader->lengthInSamples, 0, true, true);
                
                loadedSampleRate = reader->sampleRate;
                
                // Load into Transport for playback
                readerSource.reset(new juce::AudioFormatReaderSource(readerPtr.release(), true));
                transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
                
                thumbnail.setSource(new juce::FileInputSource(file));
                fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                
                // Auto Detect Pitch
                detectedFrequency = analyzer.detectPitch(loadedAudio, loadedSampleRate);
                pitchEditor.setText(juce::String(detectedFrequency, 2), juce::dontSendNotification);
                updateGridIndicator();

                analyzeButton.setEnabled(true);
                playOriginalButton.setEnabled(true);
                playModelButton.setEnabled(false); // Enable only after analysis
                repaint();
            }
        }
    });
}

// 2026-09-24: INDICADOR DE REJILLA. Feedback inmediato tras cargar o
// grabar material: la f0 del ajuste por minimos cuadrados y su residuo
// RMS en cents sobre los picos de todas las ventanas, pintado por las BANDAS
// del analizador (verde < residualGreenCents: el material es una rejilla;
// amarillo < residualAmberCents; naranja por encima).
void MainComponent::updateGridIndicator()
{
    // 2026-09-27: el MODELO PURO (Analysis/GridIndicator.h) escribe el texto y
    // decide el color; aqui solo se RECOGE la entrada (lo que la GUI ya tiene
    // medido) y se PINTA lo que el modelo devuelve. El modulo tiene SU test
    // (NEURONiK_GridIndicatorTest): la GUI deja de ser la unica que verifica.
    Analysis::GridIndicatorModel indicator;
    indicator.detectedHz = detectedFrequency;
    indicator.residualCents = analyzer.lastGridResidualCents();
    indicator.observations = analyzer.lastGridObservations();
    indicator.fixedGrid = fixedGridButton.getToggleState();
    indicator.offsetsTranspose = offsetsTransposeButton.getToggleState();
    indicator.belowEstimatorFloor =
        detectedFrequency < NEURONiK::ModelMaker::Analysis::SpectralAnalyzer::anchorFloorHz (loadedSampleRate);

    if (indicator.detectedHz <= 0.0)
    {
        // Sin material no hay nada que cargar: el indicador sale del ciclo de
        // Tab (un activable sin accion no debe robar un paso de teclado).
        gridLabel.setWantsKeyboardFocus (false);
        gridLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    gridLabel.setWantsKeyboardFocus (true);   // hay material: Tab lo alcanza

    const juce::String text (indicator.text());

    // Los avisos de modo (FIJA, TRANSPONIBLES, "fijada a mano") YA viajan
    // dentro del texto del modelo, en el orden de siempre — y con ellos la
    // razon de cada uno: FIJA anuncia que no hay seguimiento por ventana, y
    // "fijada a mano" que el HPS no llega a leer esa f0 (el residuo que
    // acompana SI es el de ESA rejilla).
    gridLabel.setText (text, juce::dontSendNotification);

    // Las BANDAS viven en el MODELO (GridIndicator.h; el analizador las
    // aliasa): el pincel solo lee el RGBA que devuelve, y el test de rangos
    // del banco CZ101 sigue midiendo el mismo numero de la misma fuente.
    gridLabel.setColour (juce::Label::textColourId, juce::Colour (indicator.argb()));
}

// 2026-09-25: CLIC EN EL INDICADOR DE REJILLA -> la f0 que el detector midio
// viaja al editor de pitch. Es la MISMA pareja que usa el camino de grabar
// (texto a 2 decimales + nota/octava), y por eso `updateRootNoteFromFreq` es
// seguro aqui: escribe los combos sin notificacion, asi que NO hay ida y vuelta
// que cuantice la rejilla a la nota mas cercana — que es justo lo que hay que
// evitar (64,50 Hz de rejilla no es 65,41 de C2). No re-analiza: cargar el dato
// y analizar son dos decisiones distintas, y el unico que analiza sigue siendo
// el boton Analizar.
void MainComponent::useDetectedFrequency()
{
    if (detectedFrequency <= 0.0f)
        return;   // sin material cargado no hay rejilla que ofrecer

    pitchEditor.setText (juce::String (detectedFrequency, 2), juce::dontSendNotification);
    updateRootNoteFromFreq (detectedFrequency);
    pitchEditor.grabKeyboardFocus();
}

void MainComponent::mouseUp (const juce::MouseEvent& e)
{
    // El listener va atado al indicador, asi que el evento ya viene de ahi; el
    // guardia de arrastre evita que un arrastre que ACABA encima cuente como
    // clic (es el mismo criterio que usa el propio Label para editarse).
    if (e.eventComponent == &gridLabel && ! e.mouseWasDraggedSinceMouseDown())
    {
        useDetectedFrequency();
        return;
    }

    juce::Component::mouseUp (e);
}

// 2026-09-27: TECLADO — Space/Enter sobre el indicador (enfocado con Tab)
// ACTUAN el clic, la convencion de JUCE para lo activable con teclado
// (Button::keyPressed). El guard de material vive en useDetectedFrequency, asi
// que sin f0 la tecla no hace nada — igual que el clic. El guard de foco
// evita robar la tecla a quien la este usando (el editor de pitch, p. ej.).
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (gridLabel.hasKeyboardFocus (false)
        && (key.isKeyCode (juce::KeyPress::spaceKey) || key.isKeyCode (juce::KeyPress::returnKey)))
    {
        useDetectedFrequency();
        return true;
    }

    return juce::Component::keyPressed (key);
}

// 2026-09-27: el ANILLO DE FOCO del indicador. El foco JUCE no pinta nada por
// si solo: sin esto, Tab enfoca el label y NADA en pantalla lo delata.
// paintOverChildren lo dibuja SOLO cuando el label tiene el foco (encima de
// los hijos, sin tocar su pintura); focusStateChangedCallback lo mantiene al
// dia al entrar y al salir.
void MainComponent::paintOverChildren (juce::Graphics& g)
{
    if (! gridLabelHasFocus)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.drawRoundedRectangle (gridLabel.getBounds().toFloat().expanded (2.5f), 4.0f, 1.5f);
}

void MainComponent::focusStateChangedCallback()
{
    const bool hasFocus = gridLabel.hasKeyboardFocus (false);

    if (hasFocus != gridLabelHasFocus)
    {
        gridLabelHasFocus = hasFocus;
        repaint();
    }
}

void MainComponent::analyzeAudio()
{
    if (loadedAudio.getNumSamples() == 0) return;

    float f0 = pitchEditor.getText().getFloatValue();
    if (f0 < 20.0f) f0 = 130.81f; 

    // MODO REJILLA FIJA (2026-09-25): declarado por el usuario. La rejilla es
    // suya y el analisis no la cambia (ni la pule con minimos cuadrados, ni la
    // sigue por ventana en el analisis temporal), y el modelo lo declara.
    const bool fixedGrid = fixedGridButton.getToggleState();

    // SUELO DEL ESTIMADOR (2026-09-25): por debajo, el HPS no puede leer la
    // raiz del material (E1 = 41.62 Hz: lee su 2o armonico — 501 cents de error
    // acustico medidos con la rejilla automatica). La rejilla escrita a mano
    // manda: el ajuste por minimos cuadrados la PULE sobre los picos de todas
    // las ventanas (y publica SU residuo para el indicador), y ni el chequeo de
    // discrepancia la pisa ni el analisis temporal la re-estima por ventana.
    // Con el modo REJILLA FIJA no se pule nada: la rejilla declarada se usa tal
    // cual (su procedencia ya no es "lo que diga el ajuste").
    const bool belowFloor = f0 < NEURONiK::ModelMaker::Analysis::SpectralAnalyzer::anchorFloorHz (loadedSampleRate);
    if (belowFloor && ! fixedGrid)
    {
        f0 = analyzer.refineGrid (loadedAudio, loadedSampleRate, f0);
        detectedFrequency = f0;
        updateGridIndicator();
    }

    // Discrepancy check
    int noteId = noteCombo.getSelectedId();
    int octId = octaveCombo.getSelectedId();
    
    if (! fixedGrid && ! belowFloor && noteId > 1 && octId > 1) 
    {
        int selectedMidi = (octId - 4 + 2) * 12 + (noteId - 2);
        float rawDetectF = analyzer.detectPitch(loadedAudio, loadedSampleRate);
        int rawMidi = juce::roundToInt(12.0 * std::log2(rawDetectF / 440.0) + 69.0);
        
        if (std::abs(rawMidi - selectedMidi) > 1)
        {
             juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                    "Pitch Discrepancy",
                                                    "Detected: " + juce::String(rawDetectF, 1) + " Hz\n" +
                                                    "Selected Note: " + juce::MidiMessage::getMidiNoteName(selectedMidi, true, true, 3) + "\n\n" +
                                                    "Syncing to detected pitch for accuracy.",
                                                    "OK");
             
             f0 = rawDetectF;
             pitchEditor.setText(juce::String(f0, 2), juce::dontSendNotification);
             updateRootNoteFromFreq(f0);
             detectedFrequency = rawDetectF;
             updateGridIndicator();
        }
    }

    performAnalysis(f0, fixedGrid);
}

int MainComponent::selectedFrameCount() const
{
    const int id = framesCombo.getSelectedId();
    return id > 1 ? id : 1;
}

void MainComponent::performAnalysis(float f0, bool fixedGrid)
{
    // FASE 10.4: el analisis temporal produce N frames cuando el usuario lo
    // pide; con 1 frame la llamada es equivalente al analisis estatico.
    // REJILLA FIJA (2026-09-25): el modo declarado viaja al analizador, que deja
    // de seguir el pitch por ventana y lo declara en el modelo (gridFixed).
    const int nFrames = selectedFrameCount();
    currentModel = (nFrames <= 1)
        ? analyzer.analyze(loadedAudio, loadedSampleRate, f0, fixedGrid)
        : analyzer.analyzeTemporal(loadedAudio, loadedSampleRate, f0, nFrames, fixedGrid);
    // GUARDIA (2026-09-23): aviso cuando el material no es cuasi-monotonico y
    // el modelo estatico saldria des-afinado. El analisis temporal (frames)
    // sigue el pitch por ventana y no dispara la guardia.
    if (nFrames <= 1)
    {
        const float guard = analyzer.lastPitchGuardCents();

        // RESIDUO (2026-09-25): el aviso dice CUANTO se ha movido el pitch; el
        // residuo dice de QUE clase es el material. Se cita el ULTIMO residuo
        // medido (detectPitch / refineGrid sobre ESTE mismo buffer, el que la
        // fila de rejilla ya esta enseñando) en vez de recalcularlo: una sola
        // medida, dos sitios que la citan, y ninguna forma de que el aviso y el
        // indicador de rejilla se contradigan. Va pegado al aviso porque es lo
        // que separa "una rejilla que se mueve" (residuo bajo: el material SI
        // es armonico, el problema es el movimiento) de "el material no es una
        // rejilla" (residuo alto: offsets, ruido o barrido). El TEXTO de la cita
        // es uno solo (analyzer.gridResidualNotice): esta fila, el dialogo que
        // bloquea la exportacion y la sonda RealWav escriben la MISMA frase.
        juce::String guardText;

        if (guard > 0.0f)
        {
            guardText = "Pitch inestable (" + juce::String (guard, 0)
                            + " cents): el modelo estatico quedaria des-afinado; usa mas frames";

            // La frase del residuo sale del analizador (gridResidualNotice): es
            // la MISMA que citan el dialogo que bloquea la exportacion y el
            // reporte de la sonda RealWav.
            guardText += "  |  " + analyzer.gridResidualNotice();
        }

        // La fila del aviso existe SOLO mientras hay aviso (ver resized()): al
        // encenderse o apagarse hay que re-colocar, no basta con repintar.
        const bool hadGuard = pitchGuardLabel.getText().isNotEmpty();
        pitchGuardLabel.setText (guardText, juce::dontSendNotification);

        if (hadGuard != guardText.isNotEmpty())
            resized();
    }
    else
    {
        const bool hadGuard = pitchGuardLabel.getText().isNotEmpty();
        pitchGuardLabel.setText ({}, juce::dontSendNotification);

        if (hadGuard)
            resized();
    }

    
    // 2026-09-25: OFFSETS TRANSPONIBLES — el modo se re-declara en cada
    // analisis con el estado del toggle (no cambia el analisis, cambia como
    // lo lee el motor al transportar).
    currentModel.offsetsTranspose = offsetsTransposeButton.getToggleState();

    // Prepare Preview
    previewResonator.loadModel(currentModel, 0); // Load into slot A
    previewResonator.setBaseFrequency(f0);
    
    playModelButton.setEnabled(true);
    exportButton.setEnabled(true);

    // FASE 11.5: los DATOS DE VISTA de las capas (que indice fue a parar a que
    // capa, su traza y la leyenda) que pinta la pestana CAPAS.
    // FASE 11.4: el VEREDICTO de la puerta de plegado (10.3) viaja DENTRO de
    // la vista. En temporal es la medida real del analizador
    // (lastOctaveFold()); en estatico la puerta no se mide (el analyzer solo
    // la lleva en analyzeTemporal), asi que el panel lo dira sin inventar.
    currentLayerView = (nFrames <= 1)
        ? Analysis::buildLayerView(currentModel)
        : Analysis::buildLayerView(currentModel, analyzer.lastOctaveFold());

    repaint();
}

void MainComponent::exportModel()
{

    // GUARDIA (2026-09-23): bloquear la exportacion de un modelo estatico
    // des-afinado (material con desviacion de pitch). El temporal exporta.
    if (currentModel.frameCount <= 1 && analyzer.lastPitchGuardCents() > 0.0f)
    {
        // 2026-09-26: el dialogo cita el residuo con la MISMA frase que la
        // fila del aviso y que la sonda RealWav
        // (SpectralAnalyzer::gridResidualNotice): cuanto se movio el pitch y de
        // que clase es el material son dos medidas del MISMO analisis, y el
        // usuario las lee igual en las tres superficies.
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Modelo des-afinado",
            "El material tiene una desviacion de pitch de "
                + juce::String (analyzer.lastPitchGuardCents(), 0)
                + " cents: un modelo estatico no lo representa. Genera un modelo "
                  "temporal (mas frames) o usa material cuasi-monotonico.\n\n"
                + "Rejilla del analisis: " + analyzer.gridResidualNotice() + ".");
        return;
    }

    // FASE 11.1: el DIALECTO (v2 / v2.1) vive en un solo sitio —
    // Common::writeModelToFile (Source/Common/SpectralModelWriter.h). Antes esta
    // funcion tenia su copia, el test de roundtrip otra y la sonda RealWav una
    // tercera: tres fuentes de verdad para un formato que crece. Aqui queda solo
    // la POLITICA de la GUI (el nombre, la guardia de afinacion de arriba y el
    // dialogo de fichero). El modelo se copia porque el guardado ocurre DESPUES
    // del dialogo, en su callback asincrono.
    const Common::SpectralModel modelToWrite = currentModel;
    const juce::String modelName = fileNameLabel.getText();

    juce::File startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                          .getChildFile("NEURONiK").getChildFile("Models").getChildFile("User");

    juce::String lastPath = loadSetting("lastExportPath");
    if (lastPath.isNotEmpty())
        startDir = juce::File(lastPath);

    // Default filename derived from loaded audio
    juce::String defaultFileName = "model.neuronikmodel";
    juce::String currentText = fileNameLabel.getText();
    if (currentText.isNotEmpty() && currentText != "No file loaded")
    {
        defaultFileName = juce::File(currentText).getFileNameWithoutExtension() + ".neuronikmodel";
    }

    fileChooser = std::make_unique<juce::FileChooser>("Save Model",
                                                     startDir.getChildFile(defaultFileName),
                                                     "*.neuronikmodel");

    auto browserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(browserFlags, [modelToWrite, modelName, this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            saveSetting("lastExportPath", file.getParentDirectory().getFullPathName());
            Common::writeModelToFile(file, modelToWrite, modelName,
                                     "Created with NEURONiK Model Maker");
        }
    });
}

void MainComponent::paintSpectralView(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Draw 64 bars
    float barWidth = (float)area.getWidth() / 64.0f;
    float maxBarHeight = (float)area.getHeight();
    
    for (int i = 0; i < 64; ++i)
    {
        float amp = currentModel.amplitudes[i]; // 0 to 1
        float barHeight = amp * maxBarHeight;
        
        juce::Rectangle<float> bar(
            area.getX() + i * barWidth,
            area.getBottom() - barHeight,
            barWidth - 1.0f,
            barHeight
        );
        
        g.setColour(juce::Colours::orange.withAlpha(0.8f));
        g.fillRect(bar);
        
        // Reflection/Glow
        if (amp > 0.1f)
        {
            g.setColour(juce::Colours::orange.withAlpha(0.3f));
            g.fillRect(bar.withHeight(2.0f).translated(0, -2.0f));
        }
    }
}

void MainComponent::paintLayerView(juce::Graphics& g, juce::Rectangle<int> area)
{
    // FASE 11.5: el feedback de POR QUE el analizador partio (o no) el material.
    // Una columna por parcial (n=1..64): la ALTURA es su pico normalizado al
    // GLOBAL y el COLOR su capa — la traza de la envolvente por frame va encima.
    using Analysis::LayerView;

    const auto& view = currentLayerView;

    const int legendH = juce::jlimit (16, 30, area.getHeight() / 5);
    // FASE 11.4: la fila del VEREDICTO de la puerta, encima de la leyenda.
    const int verdictH = juce::jlimit (12, 18, area.getHeight() / 14);
    auto barsArea = area.withTrimmedBottom (legendH + verdictH);

    const float colW = (float) barsArea.getWidth() / (float) LayerView::kMaxPartials;
    const float maxH = (float) barsArea.getHeight();

    // Paleta por capa: la 0 es la RAIZ (lo que suena sin motor de capas).
    const juce::Colour layerColour[3] = {
        juce::Colours::cyan, juce::Colour (0xFFFF6EC7), juce::Colour (0xFFFFD24A)
    };

    for (int i = 0; i < LayerView::kMaxPartials; ++i)
    {
        const float x = (float) barsArea.getX() + (float) i * colW;
        const int layer = view.layerOfPartial[(size_t) i];

        if (layer < 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (x, (float) barsArea.getBottom() - 2.0f, colW - 1.0f, 2.0f);
            continue;
        }

        const auto colour = layerColour[(size_t) juce::jlimit (0, 2, layer)];
        const float h = juce::jmax (2.0f, view.peakOfPartial[(size_t) i] * maxH);

        g.setColour (colour.withAlpha (0.35f));
        g.fillRect (x, (float) barsArea.getBottom() - h, colW - 1.0f, h);

        // La traza (la FORMA de la envolvente, normalizada a su pico), si la
        // columna la admite: con 64 columnas solo cabe cuando hay sitio.
        const int frames = view.frameCountOf (i);

        if (colW >= 4.0f && frames > 1)
        {
            juce::Path trace;

            for (int f = 0; f < frames; ++f)
            {
                const float px = x + (float) f / (float) (frames - 1) * (colW - 1.0f);
                const float py = (float) barsArea.getBottom()
                                     - h * juce::jlimit (0.0f, 1.0f, view.envelope[(size_t) i][(size_t) f]);

                if (f == 0) trace.startNewSubPath (px, py);
                else        trace.lineTo (px, py);
            }

            g.setColour (colour.withAlpha (0.95f));
            g.strokePath (trace, juce::PathStrokeType (1.0f));
        }
    }

    // FASE 11.4: el VEREDICTO de la puerta de plegado (10.3) — la razon del
    // reparto no se adivina. Es la MISMA medida que decide el clustering:
    // mono-rejilla (dispersion post-plegado bajo el corte de 100 cents) o
    // bi-rejilla (raices genuinamente distintas, material exento).
    {
        g.setFont (juce::Font (juce::FontOptions ((float) verdictH)));
        const auto band = area.removeFromBottom (legendH + verdictH).removeFromTop (verdictH);

        if (! view.fold.measured)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawText ("puerta de plegado: sin medida (el analisis estatico no la mide)",
                        band, juce::Justification::centredLeft, true);
        }
        else
        {
            // Los mismos colores del panel: ambar = exento (bi), verde suave = mono.
            g.setColour ((view.fold.biGrid ? juce::Colour (0xFFFFD24A) : juce::Colour (0xFF6EE7B7))
                             .withAlpha (0.9f));

            const juce::String verdictText = view.fold.biGrid
                ? ("bi-rejilla: raices genuinamente distintas ("
                   + juce::String (view.fold.foldCents, 1) + " cents post-plegado, "
                   + juce::String (view.fold.rawCents, 1) + " crudos) — exento de clustering")
                : ("mono-rejilla: una sola rejilla (" + juce::String (view.fold.foldCents, 1)
                   + " cents post-plegado"
                   + (view.fold.octaveFlips > 0
                          ? ", " + juce::String (view.fold.octaveFlips)
                                + " salto(s) de octava del estimador plegado(s) a acuerdo"
                          : "")
                   + ", " + juce::String (view.fold.observations) + " ventanas)");

            g.drawText (verdictText, band, juce::Justification::centredLeft, true);
        }
    }

    // Leyenda: un bloque por capa (cuadrado de color + nombre + indices).
    auto legend = area.removeFromBottom (legendH);
    g.setFont (juce::Font (juce::FontOptions ((float) juce::jlimit (9, 13, legendH - 5))));

    if (! view.layered())
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText ("mono-rejilla: el analisis no partio el material",
                    legend, juce::Justification::centredLeft, true);
    }
    else
    {
        const int cellW = legend.getWidth() / juce::jmax (1, view.layerCount);

        for (int l = 0; l < view.layerCount; ++l)
        {
            const auto cell = legend.withX (legend.getX() + l * cellW).withWidth (cellW).reduced (2, 0);
            const auto& info = view.layers[(size_t) l];
            const auto label = (info.name.isNotEmpty() ? info.name : ("capa " + juce::String (l + 1)))
                                   + " (" + juce::String (info.partials) + " idx, peso "
                                   + juce::String (info.weight, 2) + ")";

            const int swatch = juce::jlimit (6, 12, cell.getHeight() - 2);
            g.setColour (layerColour[(size_t) juce::jlimit (0, 2, l)]);
            g.fillRect (cell.getX(), cell.getCentreY() - swatch / 2, swatch, swatch);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (label, cell.withTrimmedLeft (swatch + 4), juce::Justification::centredLeft, true);
        }
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    if (area.isEmpty()) return;

    // Fixed base dimension for proportions
    const float baseW = 800.0f;
    const float baseH = 600.0f;
    float scaleX = (float)area.getWidth() / baseW;
    float scaleY = (float)area.getHeight() / baseH;
    float scale = juce::jmin(scaleX, scaleY);
    if (scale < 0.5f) scale = 0.5f;

    // Menu Bar
    menuBar.setBounds(area.removeFromTop(juce::roundToInt(24.0f * scaleY)));

    area.reduce(juce::roundToInt(20.0f * scaleX), juce::roundToInt(20.0f * scaleY)); 

    // Header
    auto headerArea = area.removeFromTop(juce::roundToInt(40.0f * scaleY));
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f * scale).withStyle("Bold")));
    titleLabel.setBounds(headerArea.removeFromLeft(juce::roundToInt(300.0f * scaleX)));
    
    exportButton.setBounds(headerArea.removeFromRight(juce::roundToInt(120.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));
    headerArea.removeFromRight(juce::roundToInt(20.0f * scaleX)); 
    loadButton.setBounds(headerArea.removeFromRight(juce::roundToInt(100.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));
    fileNameLabel.setBounds(headerArea);

    gridLabel.setBounds (area.removeFromTop (juce::roundToInt (20.0f * scaleY))); // fila: indicador de rejilla

    // AVISO DE PITCH (2026-09-25): fila PROPIA de ancho completo, y solo
    // mientras hay aviso. En el header compartia renglon con el nombre del
    // fichero (~220 px de ancho a 800): el aviso YA se recortaba a media frase
    // y con el residuo detras no habria cabido nunca. Sin aviso la fila no se
    // descuenta, asi que el area de abajo no pierde un pixel.
    if (pitchGuardLabel.getText().isNotEmpty())
        pitchGuardLabel.setBounds (area.removeFromTop (juce::roundToInt (20.0f * scaleY)));
    else
        pitchGuardLabel.setBounds ({});

    // Footer
    auto footerArea = area.removeFromBottom(juce::roundToInt(40.0f * scaleY));
    
    // Left side: Playback & Record
    recordButton.setBounds(footerArea.removeFromLeft(juce::roundToInt(60.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));
    footerArea.removeFromLeft(juce::roundToInt(10.0f * scaleX));
    playOriginalButton.setBounds(footerArea.removeFromLeft(juce::roundToInt(120.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));
    footerArea.removeFromLeft(juce::roundToInt(10.0f * scaleX));
    playModelButton.setBounds(footerArea.removeFromLeft(juce::roundToInt(120.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));
    footerArea.removeFromLeft(juce::roundToInt(10.0f * scaleX));
    stopButton.setBounds(footerArea.removeFromLeft(juce::roundToInt(80.0f * scaleX)).reduced(0, juce::roundToInt(4.0f * scaleY)));

    // Right side: Analysis & Pitch
    analyzeButton.setBounds(footerArea.removeFromRight(juce::roundToInt(120.0f * scaleX)).reduced(0, juce::roundToInt(2.0f * scaleY)));
    framesCombo.setBounds(footerArea.removeFromRight(juce::roundToInt(110.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    footerArea.removeFromRight(juce::roundToInt(20.0f * scaleX));
    
    // Pitch & Note
    pitchEditor.setBounds(footerArea.removeFromRight(juce::roundToInt(60.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    pitchLabel.setBounds(footerArea.removeFromRight(juce::roundToInt(100.0f * scaleX)));
    fixedGridButton.setBounds(footerArea.removeFromRight(juce::roundToInt(110.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    offsetsTransposeButton.setBounds(footerArea.removeFromRight(juce::roundToInt(120.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    
    footerArea.removeFromRight(juce::roundToInt(10.0f * scaleX));
    octaveCombo.setBounds(footerArea.removeFromRight(juce::roundToInt(50.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    noteCombo.setBounds(footerArea.removeFromRight(juce::roundToInt(60.0f * scaleX)).reduced(0, juce::roundToInt(8.0f * scaleY)));
    
    // Proportional Fonts
    auto labelFont = juce::Font(juce::FontOptions(14.0f * scale));
    auto editorFont = juce::Font(juce::FontOptions(16.0f * scale).withStyle("Bold"));
    
    pitchLabel.setFont(labelFont);
    pitchEditor.setFont(editorFont);
    // noteCombo and octaveCombo fonts are handled by LookAndFeel, ComboBox doesn't have setFont()
    fileNameLabel.setFont(juce::Font(juce::FontOptions(13.0f * scale)));
    gridLabel.setFont(juce::Font(juce::FontOptions(13.0f * scale)));
    // El aviso tiene ya su propia fila: escala con el zoom como el indicador
    // de rejilla (a 0.5x el texto largo es el que mas sufre).
    pitchGuardLabel.setFont(juce::Font(juce::FontOptions(13.0f * scale)));

    // Boxes (Split remaining space in THREE: waveform, spectrum, layers)
    const int boxGap = juce::roundToInt(14.0f * scaleY);
    auto boxHeight = (area.getHeight() - 2 * boxGap) / 3;
    waveBox.setBounds(area.removeFromTop(boxHeight));
    area.removeFromTop(boxGap);
    spectralBox.setBounds(area.removeFromTop(boxHeight));
    area.removeFromTop(boxGap);
    layersBox.setBounds(area);
}

// --- Persistence Helpers ---

juce::File MainComponent::getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("NEURONiK").getChildFile("ModelMaker.xml");
}

void MainComponent::saveSetting(const juce::String& key, const juce::String& value)
{
    auto file = getSettingsFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::XmlElement> root;
    if (file.exists())
        root = juce::XmlDocument::parse(file);
    
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement>("SETTINGS");

    root->setAttribute(key, value);
    root->writeTo(file);
}

juce::String MainComponent::loadSetting(const juce::String& key)
{
    auto file = getSettingsFile();
    if (file.exists())
    {
        auto root = juce::XmlDocument::parse(file);
        if (root != nullptr)
            return root->getStringAttribute(key);
    }
    return {};
}


// --- MenuBarModel ---

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Load Audio...", true, false);
        menu.addItem(2, "Export Model...", exportButton.isEnabled(), false);
        menu.addSeparator();
        menu.addItem(3, "Exit", true, false);
    }
    else if (menuIndex == 1) // Edit
    {
        menu.addItem(4, "Analyze", analyzeButton.isEnabled(), false);
        menu.addSeparator();
        menu.addItem(5, "Play Original", playOriginalButton.isEnabled(), false);
        menu.addItem(6, "Play Model", playModelButton.isEnabled(), false);
        menu.addItem(7, "Stop", stopButton.isEnabled(), false);
        menu.addSeparator();
        
        juce::PopupMenu zoomMenu;
        zoomMenu.addItem(300, "0.5x", true, zoomScale == 0.5f);
        zoomMenu.addItem(301, "1x (Normal)", true, zoomScale == 1.0f);
        zoomMenu.addItem(302, "1.25x", true, zoomScale == 1.25f);
        zoomMenu.addItem(303, "1.5x", true, zoomScale == 1.5f);
        zoomMenu.addItem(304, "2x", true, zoomScale == 2.0f);
        zoomMenu.addItem(305, "3x", true, zoomScale == 3.0f);
        menu.addSubMenu("Zoom", zoomMenu);

        menu.addSeparator();
        menu.addItem(9, "Options...", true, false);
    }
    else if (menuIndex == 2) // Help
    {
        menu.addItem(8, "About NEURONiK Model Maker...", true, false);
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case 1: loadFile(); break;
        case 2: exportModel(); break;
        case 3: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        case 4: analyzeAudio(); break;
        case 5: playOriginal(); break;
        case 6: playModel(); break;
        case 7: stopPlayback(); break;
        case 8:
        {
            juce::String version = juce::String(NEURONIK_MODELMAKER_VERSION_MAJOR) + "." +
                                   juce::String(NEURONIK_MODELMAKER_VERSION_MINOR) + "." +
                                   juce::String(NEURONIK_MODELMAKER_VERSION_SUB);

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "About NEURONiK Model Maker",
                "NEURONiK Model Maker\n"
                "Enhanced Spectral Morphing Suite\n"
                "Version " + version + "\n\n"
                "Part of the NEURONiK Synthesizer Suite.\n"
                "(c) 2026 ABD Neural Audio",
                "OK"
            );
            break;
        }
        case 9:
        {
            juce::DialogWindow::LaunchOptions opt;
            opt.dialogTitle = "Audio Settings";
            opt.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
            opt.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager, 0, 2, 0, 2, false, false, true, false));
            opt.content->setSize(500, 300);
            opt.launchAsync();
            break;
        }
        case 300: setZoom(0.5f); break;
        case 301: setZoom(1.0f); break;
        case 302: setZoom(1.25f); break;
        case 303: setZoom(1.5f); break;
        case 304: setZoom(2.0f); break;
        case 305: setZoom(3.0f); break;
    }
}

void MainComponent::setZoom(float scale)
{
    zoomScale = juce::jlimit(1.0f, 4.0f, scale);
    saveSetting("zoomScale", juce::String(zoomScale, 1));

    // For proportional resizing, we just change the size and let resized() handle it
    setSize(juce::roundToInt(800.0f * zoomScale), juce::roundToInt(600.0f * zoomScale));
    
    // We don't use setTransform here because we have a fully responsive resized() logic.
}

} // namespace NEURONiK::ModelMaker
