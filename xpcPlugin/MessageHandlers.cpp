//Copyright (c) 2013-2015 United States Government as represented by the Administrator of the
//National Aeronautics and Space Administration. All Rights Reserved.
//
//X-Plane API
//Copyright(c) 2008, Sandy Barbour and Ben Supnik All rights reserved.
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
//associated documentation files(the "Software"), to deal in the Software without restriction,
//including without limitation the rights to use, copy, modify, merge, publish, distribute,
//sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Neither the names of the authors nor that of X - Plane or Laminar Research
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission from the authors or
//    Laminar Research, respectively.
#include "MessageHandlers.h"
#include "DataManager.h"
#include "Drawing.h"
#include "Log.h"

#include "XPLMUtilities.h"

#include <cmath>
#include <cstring>

namespace XPC
{
	std::map<std::string, MessageHandlers::ConnectionInfo> MessageHandlers::connections;
	std::map<std::string, MessageHandler> MessageHandlers::handlers;

	std::string MessageHandlers::connectionKey;
	MessageHandlers::ConnectionInfo MessageHandlers::connection;
	UDPSocket* MessageHandlers::sock;

	void MessageHandlers::SetSocket(UDPSocket* socket)
	{
		MessageHandlers::sock = socket;
	}

	void MessageHandlers::HandleMessage(Message& msg)
	{
		if (handlers.size() == 0)
		{
			// Common messages
			handlers.insert(std::make_pair("CONN", MessageHandlers::HandleConn));
			handlers.insert(std::make_pair("CTRL", MessageHandlers::HandleCtrl));
			handlers.insert(std::make_pair("DATA", MessageHandlers::HandleData));
			handlers.insert(std::make_pair("DREF", MessageHandlers::HandleDref));
			handlers.insert(std::make_pair("GETD", MessageHandlers::HandleGetD));
			handlers.insert(std::make_pair("POSI", MessageHandlers::HandlePosi));
			handlers.insert(std::make_pair("SIMU", MessageHandlers::HandleSimu));
			handlers.insert(std::make_pair("TEXT", MessageHandlers::HandleText));
			handlers.insert(std::make_pair("WYPT", MessageHandlers::HandleWypt));
			handlers.insert(std::make_pair("VIEW", MessageHandlers::HandleView));
			// X-Plane data messages
			handlers.insert(std::make_pair("DSEL", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("USEL", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("DCOC", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("UCOC", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("MOUS", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("CHAR", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("MENU", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("SOUN", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("FAIL", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("RECO", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("PAPT", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("VEHN", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("VEH1", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("VEHA", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("GSET", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("OBJN", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("OBJL", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("GSET", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("ISET", MessageHandlers::HandleXPlaneData));
			handlers.insert(std::make_pair("BOAT", MessageHandlers::HandleXPlaneData));
		}

		// Make sure we really have a message to handle.
		std::string head = msg.GetHead();
		if (head == "")
		{
#if LOG_VERBOSITY > 1
			Log::WriteLine("[MSGH] Warning: HandleMessage called with empty message.");
#endif
			return; // No Message to handle
		}
		msg.PrintToLog();

		// Set current connection
		sockaddr sourceaddr = msg.GetSource();
		connectionKey = UDPSocket::GetHost(&sourceaddr);
#if LOG_VERBOSITY > 4
		Log::FormatLine("[MSGH] Handling message from %s", connectionKey.c_str());
#endif
		std::map<std::string, ConnectionInfo>::iterator conn = connections.find(connectionKey);
		if (conn == connections.end()) // New connection
		{
			connection = MessageHandlers::ConnectionInfo();
			// If this is a new connection, that means we just added an elment
			// to connections. As long as we never remove elements, the size of
			// connections will serve as a unique id.
			connection.id = static_cast<unsigned char>(connections.size());
			connection.addr = sourceaddr;
			connection.getdCount = 0;
			connections[connectionKey] = connection;
#if LOG_VERBOSITY > 2
			Log::FormatLine("[MSGH] New connection. ID=%u, Remote=%s", connection.id, connectionKey.c_str());
#endif
		}
		else
		{
			connection = (*conn).second;
#if LOG_VERBOSITY > 3
			Log::FormatLine("[MSGH] Existing connection. ID=%u, Remote=%s",
				connection.id, connectionKey.c_str());
#endif
		}

		// Check if there is a handler for this message type. If so, execute
		// that handler. Otherwise, execute the unknown message handler.
		std::map<std::string, MessageHandler>::iterator iter = handlers.find(head);
		if (iter != handlers.end())
		{
			MessageHandler handler = (*iter).second;
			handler(msg);
		}
		else
		{
			MessageHandlers::HandleUnknown(msg);
		}
	}

	void MessageHandlers::HandleConn(const Message& msg)
	{
		const unsigned char* buffer = msg.GetBuffer();

		// Store new port
		unsigned short port = *((unsigned short*)(buffer + 5));
		sockaddr* sa = &connection.addr;
		switch (sa->sa_family)
		{
		case AF_INET: // IPV4 address
		{
			sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(sa);
			(*sin).sin_port = htons(port);
			break;
		}
		case AF_INET6: // IPV6 addres
		{
			sockaddr_in6* sin = reinterpret_cast<sockaddr_in6*>(sa);
			(*sin).sin6_port = htons(port);
			break;
		}
		default:
#if LOG_VERBOSITY > 0
			Log::WriteLine("[CONN] ERROR: Unknown address type.");
			return;
#endif
		}
		connections.erase(connectionKey);
		connectionKey = UDPSocket::GetHost(&connection.addr);
		connections[connectionKey] = connection;

		// Create response
		unsigned char response[6] = "CONF";
		response[5] = connection.id;

		// Update log
#if LOG_VERBOSITY > 1
		Log::FormatLine("[CONN] ID: %u New destination port: %u",
			connection.id, port);
#endif

		// Send response
		sock->SendTo(response, 6, &connection.addr);
	}

	void MessageHandlers::HandleCtrl(const Message& msg)
	{
		// Update Log
#if LOG_VERBOSITY > 2
		Log::FormatLine("[CTRL] Message Received (Conn %i)", connection.id);
#endif

		const unsigned char* buffer = msg.GetBuffer();
		std::size_t size = msg.GetSize();
		//Legacy packets that don't specify an aircraft number should be 26 bytes long.
		//Packets specifying an A/C num should be 27 bytes.
		if (size != 26 && size != 27 && size != 31)
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[CTRL] ERROR: Unexpected message length (%i)", size);
#endif
			return;
		}

		// Parse message data
		float pitch = *((float*)(buffer + 5));
		float roll = *((float*)(buffer + 9));
		float yaw = *((float*)(buffer + 13));
		float throttle = *((float*)(buffer + 17));
		char gear = buffer[21];
		float flaps = *((float*)(buffer + 22));
		unsigned char aircraftNumber = 0;
		if (size >= 27)
		{
			aircraftNumber = buffer[26];
		}
		float spdbrk = -998;
		if (size >= 31)
		{
			spdbrk = *((float*)(buffer + 27));
		}


		if (pitch < -999.5 || pitch > -997.5)
		{
			DataManager::Set(DREF_YokePitch, pitch, aircraftNumber);
		}
		if (roll < -999.5 || roll > -997.5)
		{
			DataManager::Set(DREF_YokeRoll, roll, aircraftNumber);
		}
		if (yaw < -999.5 || yaw > -997.5)
		{
			DataManager::Set(DREF_YokeHeading, yaw, aircraftNumber);
		}
		if (throttle < -999.5 || throttle > -997.5)
		{

			float throttleArray[8];
			for (int i = 0; i < 8; ++i)
			{
				throttleArray[i] = throttle;
			}
			DataManager::Set(DREF_ThrottleSet, throttleArray, 8, aircraftNumber);
			DataManager::Set(DREF_ThrottleActual, throttleArray, 8, aircraftNumber);
			if (aircraftNumber == 0)
			{
				DataManager::Set("sim/flightmodel/engine/ENGN_thro_override", throttleArray, 1);
			}
		}
		if (gear != -1)
		{
			DataManager::SetGear(gear, false, aircraftNumber);
		}
		if (flaps < -999.5 || flaps > -997.5)
		{
			DataManager::Set(DREF_FlapSetting, flaps, aircraftNumber);
		}
		if (spdbrk < -999.5 || spdbrk > -997.5)
		{
			DataManager::Set(DREF_SpeedBrakeSet, spdbrk, aircraftNumber);
		}
	}

	void MessageHandlers::HandleData(const Message& msg)
	{
		// Parse data
		const unsigned char* buffer = msg.GetBuffer();
		std::size_t size = msg.GetSize();
		std::size_t numCols = (size - 5) / 36;
		if (numCols > 0)
		{
#if LOG_VERBOSITY > 1
			Log::FormatLine("[DATA] Message Received (Conn %i)", connection.id);
#endif
		}
		else
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[DATA] WARNING: Empty data packet received (Conn %i)", connection.id);
#endif
			return;
		}

		if (numCols > 134) // Error. Will overflow values
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[DATA] ERROR: numCols to large.");
#endif
			return;
		}
		float values[134][9];
		for (int i = 0; i < numCols; ++i)
		{
			// 5 byte header + (9 * 4 = 36) bytes per row
			values[i][0] = buffer[5 + 36 * i];
			memcpy(values[i] + 1, buffer + 9 + 36 * i, 9 * sizeof(float));
		}

		// Update log

		float savedAlpha = -998;
		float savedHPath = -998;
		for (int i = 0; i < numCols; ++i)
		{
			unsigned char dataRef = (unsigned char)values[i][0];
			if (dataRef >= 134)
			{
#if LOG_VERBOSITY > 0
				Log::FormatLine("[DATA] ERROR: DataRef # must be between 0 - 134 (Received: %hi)", (int)dataRef);
#endif
				continue;
			}

			switch (dataRef)
			{
			// TODO(jason-watkins): This currently overwrites the velocity several times. Should look into making this case smarter somehow.
			case 3: // Velocity
			{
				float theta = DataManager::GetFloat(DREF_Pitch);
				float alpha = savedAlpha != -998 ? savedAlpha : DataManager::GetFloat(DREF_AngleOfAttack);
				float hpath = savedHPath != -998 ? savedHPath : DataManager::GetFloat(DREF_HPath);
				if (alpha != alpha || hpath != hpath)
				{
#if LOG_VERBOSITY > 0
					Log::WriteLine("[DATA] ERROR: Value must be a number (NaN received)");
#endif
					break;
				}
				const float deg2rad = 0.0174532925F;
				int ind[3] = { 1, 3, 4 };
				for (int j = 0; j < 3; ++j)
				{
					float v = values[i][ind[j]];
					if (v != -998)
					{
						DataManager::Set(DREF_LocalVX, v*cos((theta - alpha)*deg2rad)*sin(hpath*deg2rad));
						DataManager::Set(DREF_LocalVY, v*sin((theta - alpha)*deg2rad));
						DataManager::Set(DREF_LocalVZ, -v*cos((theta - alpha)*deg2rad)*cos(hpath*deg2rad));
					}
				}
				break;
			}
			case 17: // Orientation
			{
				float orientation[3];
				orientation[0] = values[i][1];
				orientation[1] = values[i][2];
				orientation[2] = values[i][3];
				DataManager::SetOrientation(orientation);
				break;
			}
			case 18: // Alpha, hpath etc.
			{
				if (values[i][1] != values[i][1] || values[i][3] != values[i][3])
				{
#if LOG_VERBOSITY > 0
					Log::WriteLine("[DATA] ERROR: Value must be a number (NaN received)");
#endif
					break;
				}
				if (values[i][1] != -998)
				{
					savedAlpha = values[i][1];
				}
				if (values[i][3] != -998)
				{
					savedHPath = values[i][3];
				}
				break;
			}
			case 20: // Position
			{
				float pos[3];
				pos[0] = values[i][2];
				pos[1] = values[i][3];
				pos[2] = values[i][4];
				DataManager::SetPosition(pos);
				break;
			}
			case 25: // Throttle
			{
				if (values[i][1] != values[i][1])
				{
#if LOG_VERBOSITY > 0
					Log::WriteLine("[DATA] ERROR: Value must be a number (NaN received)");
#endif
					break;
				}
				float throttle[8];
				for (int j = 0; j < 8; ++j)
				{
					throttle[j] = values[i][1];
				}
				DataManager::Set(DREF_ThrottleSet, throttle, 8);
				break;
			}
			default: // Non-Special dataRefs
			{
				float line[8];
				memcpy(line, values[i] + 1, 8 * sizeof(float));
				for (int j = 0; j < 8; ++j)
				{
#if LOG_VERBOSITY > 1
					Log::FormatLine("[DATA] Setting Dataref %i.%i to %f", dataRef, j, line[j]);
#endif
					// TODO(jason-watkins): Why is this a special case?
					if (dataRef == 14 && j == 0)
					{
						DataManager::SetGear(line[0], true);
						continue;
					}

					DREF dref = XPData[dataRef][j];
					if (dref == DREF_None)
					{
						// TODO(jason): Send single line instead!
						HandleXPlaneData(msg);
					}
					else
					{
						DataManager::Set(dref, line, 8);
					}
				}
			}
			}
		}
	}

	void MessageHandlers::HandleDref(const Message& msg)
	{
#if LOG_VERBOSITY >= 3
		Log::FormatLine("[DREF] Request to set DREF value received (Conn %i)", connection.id);
#endif
		const unsigned char* buffer = msg.GetBuffer();
		std::size_t size = msg.GetSize();
		std::size_t pos = 5;
		while (pos < size)
		{
			unsigned char len = buffer[pos++];
			if (pos + len > size)
			{
				break;
			}
			std::string dref = std::string((char*)buffer + pos, len);
			pos += len;

			unsigned char valueCount = buffer[pos++];
			if (pos + 4 * valueCount > size)
			{
				break;
			}
			float* values = (float*)(buffer + pos);
			pos += 4 * valueCount;

			DataManager::Set(dref, values, valueCount);
#if LOG_VERBOSITY >= 4
			Log::FormatLine("[DREF]     Set %d values for %s", valueCount, dref.c_str());
#endif
		}
#if LOG_VERBOSITY >= 2
		if (pos != size)
		{
			Log::WriteLine("[DREF] Error: Command did not terminate at the expected position.");
		}
#endif
	}

	void MessageHandlers::HandleGetD(const Message& msg)
	{
		const unsigned char* buffer = msg.GetBuffer();
		unsigned char drefCount = buffer[5];
		if (drefCount == 0) // Use last request
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[GETD] DATA Requested: Repeat last request from connection %i (%i data refs)",
				connection.id, connection.getdCount);
#endif
			if (connection.getdCount == 0) // No previous request to use
			{
#if LOG_VERBOSITY > 1
				Log::FormatLine("[GETD] ERROR: No previous requests from connection %i.", connection.id);
#endif
				return;
			}
		}
		else // New request
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[GETD] DATA Requested: New Request for connection %i (%i data refs)",
				connection.id, drefCount);
#endif
			std::size_t ptr = 6;
			for (int i = 0; i < drefCount; ++i)
			{
				unsigned char len = buffer[ptr];
				connection.getdRequest[i] = std::string((char*)buffer + 1 + ptr, len);
				ptr += 1 + len;
			}
			connection.getdCount = drefCount;
			connections[connectionKey] = connection;
		}

		unsigned char response[4096] = "RESP";
		response[5] = drefCount;
		std::size_t cur = 6;
		for (int i = 0; i < drefCount; ++i)
		{
			float values[255];
			int count = DataManager::Get(connection.getdRequest[i], values, 255);
			response[cur++] = count;
			memcpy(response + cur, values, count * sizeof(float));
			cur += count * sizeof(float);
		}

		sock->SendTo(response, cur, &connection.addr);
	}

	void MessageHandlers::HandlePosi(const Message& msg)
	{
		// Update log
#if LOG_VERBOSITY > 0
		Log::FormatLine("[POSI] Message Received (Conn %i)", connection.id);
#endif

		const unsigned char* buffer = msg.GetBuffer();
		const std::size_t size = msg.GetSize();
		if (size < 34)
		{
#if LOG_VERBOSITY > 1
			Log::FormatLine("[POSI] ERROR: Unexpected size: %i (Expected at least 34)", size);
#endif
			return;
		}

		char aircraftNumber = buffer[5];
		float gear = *((float*)(buffer + 30));
		float pos[3];
		float orient[3];
		memcpy(pos, buffer + 6, 12);
		memcpy(orient, buffer + 18, 12);

		if (aircraftNumber > 0)
		{
			// Enable AI for the aircraftNumber we are setting
			float ai[20];
			std::size_t result = DataManager::GetFloatArray(DREF_PauseAI, ai, 20);
			if (result == 20) // Only set values if they were retrieved successfully.
			{
				ai[aircraftNumber] = 1;
				DataManager::Set(DREF_PauseAI, ai, 0, 20);
			}
		}

		DataManager::SetPosition(pos, aircraftNumber);
		DataManager::SetOrientation(orient, aircraftNumber);
		if (gear != -1)
		{
			DataManager::SetGear(gear, true, aircraftNumber);
		}
	}

	void MessageHandlers::HandleSimu(const Message& msg)
	{
		// Update log
#if LOG_VERBOSITY > 1
		Log::FormatLine("[SIMU] Message Received (Conn %i)", connection.id);
#endif

		char v = msg.GetBuffer()[5];
		if (v < 0 || v > 2)
		{
#if LOG_VERBOSITY > 0
			Log::FormatLine("[SIMU] ERROR: Invalid argument: %i", v);
			return;
#endif
		}
		
		int value[20];
		if (v == 2)
		{
			DataManager::GetIntArray(DREF_Pause, value, 20);
			for (int i = 0; i < 20; ++i)
			{
				value[i] = value[i] ? 0 : 1;
			}
		}
		else
		{
			for (int i = 0; i < 20; ++i)
			{
				value[i] = v;
			}
		}

		// Set DREF
		DataManager::Set(DREF_Pause, value, 20);

#if LOG_VERBOSITY > 2
		switch (v)
		{
		case 0:
			Log::WriteLine("[SIMU] Simulation Resumed");
			break;
		case 1:
			Log::WriteLine("[SIMU] Simulation Paused");
			break;
		case 2:
			Log::WriteLine("[SIMU] Simulation switched.");
			break;
		}
#endif
	}

	void MessageHandlers::HandleText(const Message& msg)
	{
		// Update Log
#if LOG_VERBOSITY > 0
		Log::FormatLine("[TEXT] Message Received (Conn %i)", connection.id);
#endif

		std::size_t len = msg.GetSize();
		const unsigned char*  buffer = msg.GetBuffer();

		char text[256] = { 0 };
		if (len < 14)
		{
#if LOG_VERBOSITY > 1
			Log::WriteLine("[TEXT] ERROR: Length less than 14 bytes");
#endif
			return;
		}
		size_t msgLen = (unsigned char)buffer[13];
		if (msgLen == 0)
		{
			Drawing::ClearMessage();
#if LOG_VERBOSITY > 2
			Log::WriteLine("[TEXT] Text cleared");
#endif
		}
		else
		{
			int x = *((int*)(buffer + 5));
			int y = *((int*)(buffer + 9));
			strncpy(text, (char*)buffer + 14, msgLen);
			Drawing::SetMessage(x, y, text);
#if LOG_VERBOSITY > 2
			Log::WriteLine("[TEXT] Text set");
#endif
		}
	}

	void MessageHandlers::HandleView(const Message& msg)
	{
		// Update Log
#if LOG_VERBOSITY > 0
		Log::FormatLine("[VIEW] Message Received (Conn %i)", connection.id);
#endif

		const std::size_t size = msg.GetSize();
		if (size != 9)
		{
#if LOG_VERBOSITY > 1
			Log::FormatLine("[VIEW] Error: Unexpected length. Message was %d bytes, expected 9.", size);
#endif
			return;
		}
		const unsigned char* buffer = msg.GetBuffer();
		int type = *((int*)(buffer + 5));
		XPLMCommandKeyStroke(type);
	}

	void MessageHandlers::HandleWypt(const Message& msg)
	{
		// Update Log
#if LOG_VERBOSITY > 0
		Log::FormatLine("[WYPT] Message Received (Conn %i)", connection.id);
#endif

		// Parse data
		const unsigned char* buffer = msg.GetBuffer();
		unsigned char op = buffer[5];
		unsigned char count = buffer[6];
		Waypoint points[255];
		const unsigned char* ptr = buffer + 7;
		for (size_t i = 0; i < count; ++i)
		{
			points[i].latitude = *((float*)ptr);
			points[i].longitude = *((float*)(ptr + 4));
			points[i].altitude = *((float*)(ptr + 8));
			ptr += 12;
		}

		// Perform operation
#if LOG_VERBOSITY > 2
		Log::FormatLine("[WYPT] Performing operation %i", op);
#endif
		switch (op)
		{
		case 1:
			Drawing::AddWaypoints(points, count);
			break;
		case 2:
			Drawing::RemoveWaypoints(points, count);
			break;
		case 3:
			Drawing::ClearWaypoints();
			break;
		default:
#if LOG_VERBOSITY > 1
			Log::FormatLine("[WYPT] ERROR: %i is not a valid operation.", op);
#endif
			break;
		}
	}

	void MessageHandlers::HandleXPlaneData(const Message& msg)
	{
#if LOG_VERBOSITY > 1
		Log::WriteLine("[MSGH] Sending raw data to X-Plane");
#endif
		sockaddr_in loopback;
		loopback.sin_family = AF_INET;
		loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		loopback.sin_port = htons(49000);
		sock->SendTo(msg.GetBuffer(), msg.GetSize(), (sockaddr*)&loopback);
	}

	void MessageHandlers::HandleUnknown(const Message& msg)
	{
		// UPDATE LOG
#if LOG_VERBOSITY > 0
		Log::FormatLine("[EXEC] ERROR: Unknown packet type %s", msg.GetHead().c_str());
#endif
	}
}
