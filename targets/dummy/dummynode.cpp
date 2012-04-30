/*
	Aseba - an event-based framework for distributed robot control
	Copyright (C) 2007--2012:
		Stephane Magnenat <stephane at magnenat dot net>
		(http://stephane.magnenat.net)
		and other contributors, see authors.txt for details
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation, version 3 of the License.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ASEBA_ASSERT
#define ASEBA_ASSERT
#endif

#include "../../vm/vm.h"
#include "../../vm/natives.h"
#include "../../common/productids.h"
#include "../../common/consts.h"
#include "../../transport/buffer/vm-buffer.h"
#include <dashel/dashel.h>
#include <iostream>
#include <sstream>
#include <valarray>
#include <cassert>
#include <cstring>

class AsebaNode: public Dashel::Hub
{
private:
	AsebaVMState vm;
	std::valarray<unsigned short> bytecode;
	std::valarray<signed short> stack;
	struct Variables
	{
		sint16 id;
		sint16 source;
		sint16 args[32];
		sint16 productId;
		sint16 user[1024];
	} variables;
	
public:
	// public because accessed from a glue function
	uint16 lastMessageSource;
	std::valarray<uint8> lastMessageData;
	// this must be public because of bindings to C functions
	Dashel::Stream* stream;
	
public:
	
	AsebaNode()
	{
		// setup variables
		vm.nodeId = ASEBA_PID_UNDEFINED;
		
		bytecode.resize(512);
		vm.bytecode = &bytecode[0];
		vm.bytecodeSize = bytecode.size();
		
		stack.resize(64);
		vm.stack = &stack[0];
		vm.stackSize = stack.size();
		
		vm.variables = reinterpret_cast<sint16 *>(&variables);
		vm.variablesSize = sizeof(variables) / sizeof(sint16);
		
		// connect network
		int port = ASEBA_DEFAULT_PORT;
		try
		{
			std::ostringstream oss;
			oss << "tcpin:port=" << port;
			Dashel::Hub::connect(oss.str());
		}
		catch (Dashel::DashelException e)
		{
			std::cerr << "Cannot create listening port " << port << ": " << e.what() << std::endl;
			abort();
		}
		
		// init VM
		AsebaVMInit(&vm);
	}
	
	virtual void connectionCreated(Dashel::Stream *stream)
	{
		std::string targetName = stream->getTargetName();
		if (targetName.substr(0, targetName.find_first_of(':')) == "tcp")
		{
			std::cerr << this << " : New client connected." << std::endl;
			if (this->stream)
			{
				closeStream(this->stream);
				std::cerr << this << " : Disconnected old client." << std::endl;
			}
			this->stream = stream;
		}
	}
	
	virtual void connectionClosed(Dashel::Stream *stream, bool abnormal)
	{
		this->stream = 0;
		// clear breakpoints
		vm.breakpointsCount = 0;
		
		if (abnormal)
			std::cerr << this << " : Client has disconnected unexpectedly." << std::endl;
		else
			std::cerr << this << " : Client has disconnected properly." << std::endl;
	}
	
	virtual void incomingData(Dashel::Stream *stream)
	{
		uint16 temp;
		uint16 len;
		
		stream->read(&temp, 2);
		len = bswap16(temp);
		stream->read(&temp, 2);
		lastMessageSource = bswap16(temp);
		lastMessageData.resize(len+2);
		stream->read(&lastMessageData[0], lastMessageData.size());
		
		AsebaProcessIncomingEvents(&vm);
	}
	
	virtual void applicationStep()
	{
		// run VM
		AsebaVMRun(&vm, 65535);
		
		// reschedule a periodic event if we are not in step by step
		if (AsebaMaskIsClear(vm.flags, ASEBA_VM_STEP_BY_STEP_MASK) || AsebaMaskIsClear(vm.flags, ASEBA_VM_EVENT_ACTIVE_MASK))
			AsebaVMSetupEvent(&vm, ASEBA_EVENT_LOCAL_EVENTS_START-0);
	}
} node;

// Implementation of aseba glue code

extern "C" void AsebaPutVmToSleep(AsebaVMState *vm) 
{
	std::cerr << "Received request to go into sleep" << std::endl;
}

extern "C" void AsebaSendBuffer(AsebaVMState *vm, const uint8* data, uint16 length)
{
	Dashel::Stream* stream = node.stream;
	if (stream)
	{
		uint16 temp;
		temp = bswap16(length - 2);
		stream->write(&temp, 2);
		temp = bswap16(vm->nodeId);
		stream->write(&temp, 2);
		stream->write(data, length);
		stream->flush();
	}
}

extern "C" uint16 AsebaGetBuffer(AsebaVMState *vm, uint8* data, uint16 maxLength, uint16* source)
{
	if (node.lastMessageData.size())
	{
		*source = node.lastMessageSource;
		memcpy(data, &node.lastMessageData[0], node.lastMessageData.size());
	}
	return node.lastMessageData.size();
}

extern AsebaVMDescription nodeDescription;

extern "C" const AsebaVMDescription* AsebaGetVMDescription(AsebaVMState *vm)
{
	return &nodeDescription;
}

static AsebaNativeFunctionPointer nativeFunctions[] =
{
	ASEBA_NATIVES_STD_FUNCTIONS,
};

static const AsebaNativeFunctionDescription* nativeFunctionsDescriptions[] =
{
	ASEBA_NATIVES_STD_DESCRIPTIONS,
	0
};

extern "C" const AsebaNativeFunctionDescription * const * AsebaGetNativeFunctionsDescriptions(AsebaVMState *vm)
{
	return nativeFunctionsDescriptions;
}

extern "C" void AsebaNativeFunction(AsebaVMState *vm, uint16 id)
{
	nativeFunctions[id](vm);
}


static const AsebaLocalEventDescription localEvents[] = {
	{ "timer", "periodic timer at 50 Hz" },
	{ NULL, NULL }
};

extern "C" const AsebaLocalEventDescription * AsebaGetLocalEventsDescriptions(AsebaVMState *vm)
{
	return localEvents;
}

extern "C" void AsebaWriteBytecode(AsebaVMState *vm)
{
	std::cerr << "Received request to write bytecode into flash" << std::endl;
}

extern "C" void AsebaResetIntoBootloader(AsebaVMState *vm)
{
	std::cerr << "Received request to reset into bootloader" << std::endl;
}

extern "C" void AsebaAssert(AsebaVMState *vm, AsebaAssertReason reason)
{
	std::cerr << "\nFatal error; exception: ";
	switch (reason)
	{
		case ASEBA_ASSERT_UNKNOWN: std::cerr << "undefined"; break;
		case ASEBA_ASSERT_UNKNOWN_BINARY_OPERATOR: std::cerr << "unknown binary operator"; break;
		case ASEBA_ASSERT_UNKNOWN_BYTECODE: std::cerr << "unknown bytecode"; break;
		case ASEBA_ASSERT_STACK_OVERFLOW: std::cerr << "stack overflow"; break;
		case ASEBA_ASSERT_STACK_UNDERFLOW: std::cerr << "stack underflow"; break;
		case ASEBA_ASSERT_OUT_OF_VARIABLES_BOUNDS: std::cerr << "out of variables bounds"; break;
		case ASEBA_ASSERT_OUT_OF_BYTECODE_BOUNDS: std::cerr << "out of bytecode bounds"; break;
		case ASEBA_ASSERT_STEP_OUT_OF_RUN: std::cerr << "step out of run"; break;
		case ASEBA_ASSERT_BREAKPOINT_OUT_OF_BYTECODE_BOUNDS: std::cerr << "breakpoint out of bytecode bounds"; break;
		default: std::cerr << "unknown exception"; break;
	}
	std::cerr << ".\npc = " << vm->pc << ", sp = " << vm->sp;
	abort();
	std::cerr << "\nResetting VM" << std::endl;
	AsebaVMInit(vm);
}


int main(int argc, char* argv[])
{
	while (node.step(10))
	{
		node.applicationStep();
	}
}