# 1. Define the compiler
CXX = g++

# 2. Define compiler flags
CXXFLAGS = -Wall -std=c++17 -I/opt/homebrew/include -I./whisper.cpp -I./whisper.cpp/include -I./whisper.cpp/ggml/include

# 3. Define linker flags (Added -lcurl right in the middle)
LDFLAGS = -L/opt/homebrew/lib -lportaudio \
          -L./whisper.cpp/build/src -lwhisper \
          -L./whisper.cpp/build/ggml/src -lggml \
          -Wl,-rpath,./whisper.cpp/build/src \
          -Wl,-rpath,./whisper.cpp/build/ggml/src \
          -lcurl \
          -pthread \
          -framework Accelerate -framework Foundation -framework Metal -framework MetalKit

# 4. Define the name of the final executable program
TARGET = main

# 5. Define your source code files
SRCS = main.cpp AudioEngine.cpp AgentConfig.cpp AgentFactory.cpp ConversationMemory.cpp OllamaAgent.cpp OpenAIResponsesAgent.cpp STTModel.cpp TTSModel.cpp

# --- Build Rules ---
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
