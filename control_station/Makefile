# C++ compiler to use
CXX = g++

# Where code-gen artifacts are after they've been compiled
CODEGEN ?= $(PWD)/code-gen
CG_INSTALL = $(CODEGEN)/local_install/

# Include headers from Tangram libraries
INCLUDES = -I $(CG_INSTALL)/include \
		   -I $(CG_INSTALL)/include/tangramgenericapi/include \
		   -I $(CG_INSTALL)/include/tangramgenericapicpp \
		   -I $(CG_INSTALL)/include/tangramlmcpserializer \
		   -I $(CG_INSTALL)/include/tangramTransports/base

# Include directories where Tangram libraries can be found
LIBDIRS =  -L $(CG_INSTALL)/lib

# Link to the libraries we need
LIBS =     -l tangramgeneric \
		   -l tangramgenericapi \
		   -l tangramlmcpserializer \
		   -l tangram_transport \
		   -l zmq \
		   -l pthread

NAME = simple

# Build the executable
.PHONY: all
all: main.cpp
	$(CXX) $(INCLUDES) -o $(NAME) main.cpp $(LIBDIRS) $(LIBS)

.PHONY: clean
clean:
	rm -rf $(NAME)

