# Modular Voice Agent -> starting point

A modular C++ voice agent that listens through the microphone, transcribes speech with Whisper, routes text through a configurable agent provider, and speaks responses back through TTS.

This project started as an architecture-first build. I used Codex to help generate the initial structure and suggest the core technologies for the pipeline, then worked through the project step by step by building and testing each module as it was added.

From there, I iterated on the logic flow, tested the interaction between components, and fixed issues as they came up while the system became more orchestrated. Once the base pipeline was working, I separated the code into smaller modules to make the project cleaner and easier to extend.

That refactor made it easier to isolate the parts I expect to tune later, especially the model provider, runtime parameters, and system prompt behavior. The goal is to keep the project flexible enough to swap tools as the AI stack evolves. Long term, I want to move toward a model that can handle both speech and text more natively so the overall setup becomes simpler.

Next steps: connect the OpenAI API and compare model intelligence in a sales-style conversation flow.

Current issue: the local open-source model is not intelligent enough yet for the full behavior I want, especially when the system prompt becomes longer or more demanding.


## Architecture

The project is organized as a voice-agent loop:

`AudioEngine -> STT -> Agent -> TTS -> AudioEngine`

- `AudioEngine` handles microphone input, buffering, voice activity detection, and interruption logic.
- `STTModel` converts captured speech into text using Whisper.
- `Agent` generates a response using a selectable provider such as Ollama or OpenAI.
- `TTSModel` speaks the response aloud.
- Control returns to the audio engine for the next turn.

## Current Features

- Live microphone input with PortAudio
- Local speech-to-text with `whisper.cpp`
- Provider switching between:
  - Ollama
  - OpenAI Responses API
  - OpenAI mock provider for local testing
- Shared conversation memory across providers
- macOS text-to-speech using the built-in `say` command
- Basic interruption handling while the agent is speaking

## Project Structure

- `main.cpp` - orchestrates the full voice-agent loop
- `AudioEngine.*` - microphone queue, VAD, callback, and interrupt logic
- `STTModel.*` - speech-to-text layer
- `ConversationMemory.*` - shared multi-turn memory store
- `AgentInterface.h` - common interface for agent providers
- `AgentFactory.*` - builds the selected provider
- `AgentConfig.*` - loads runtime provider/model settings
- `OllamaAgent.*` - local Ollama backend
- `OpenAIResponsesAgent.*` - OpenAI Responses provider and mock mode
- `TTSModel.*` - speech output layer
- `system_prompt.txt` - editable system prompt
- `agent_config.txt` - provider/model configuration

## Requirements

- macOS
- `g++` with C++17 support
- PortAudio
- `libcurl`
- `whisper.cpp` checked into the project
- A Whisper model file such as `ggml-base.en.bin`

## Build

```bash
make
```

## Run

```bash
./main
```
## Cleanup

```bash
make clean
```


Press `Ctrl+C` to stop the app. The project includes a clean shutdown path so destructors and resource cleanup can run normally.

## Agent Provider Configuration

Edit `agent_config.txt` to choose the provider and model.

Example Ollama setup:

```txt
provider=ollama
model=qwen2.5:14b
temperature=0.5
max_output_tokens=125
reasoning_effort=medium
system_prompt_path=system_prompt.txt
ollama_url=http://localhost:11434/v1/chat/completions
openai_url=https://api.openai.com/v1/responses
```

Example OpenAI mock setup (test before api setup):

```txt
provider=openai_responses_mock
model=gpt-5-mini
temperature=0.5
max_output_tokens=125
reasoning_effort=medium
system_prompt_path=system_prompt.txt
ollama_url=http://localhost:11434/v1/chat/completions
openai_url=https://api.openai.com/v1/responses
```

Example real OpenAI setup:

```txt
provider=openai_responses
model=gpt-5-mini
temperature=0.5
max_output_tokens=125
reasoning_effort=medium
system_prompt_path=system_prompt.txt
ollama_url=http://localhost:11434/v1/chat/completions
openai_url=https://api.openai.com/v1/responses
```

## Ollama Setup

Start Ollama locally:

```bash
ollama serve
```

Then make sure the configured model is installed, for example:

```bash
ollama pull qwen2.5:14b
```

## OpenAI Setup

Do not place your API key in project files.

Set it in your shell environment instead:

```bash
export OPENAI_API_KEY="your_api_key_here"
```

Then switch `provider=openai_responses` in `agent_config.txt`.

## Notes

- `system_prompt.txt` is intentionally separate from the code so prompt iteration is easy.
- `.gitignore` excludes local binaries, model weights, build output, and local environment files.
- `whisper.cpp` is vendored into this repository as a project dependency.
