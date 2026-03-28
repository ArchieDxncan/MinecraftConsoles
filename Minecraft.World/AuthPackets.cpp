#include "stdafx.h"
#include "InputOutputStream.h"
#include "PacketListener.h"
#include "AuthPackets.h"

AuthPacket::AuthPacket(AuthStage stage, vector<pair<wstring, wstring>> fields)
	: stage(stage), fields(std::move(fields))
{
}

void AuthPacket::read(DataInputStream *dis)
{
	stage = static_cast<AuthStage>(dis->readByte());
	short count = dis->readShort();
	fields.clear();
	fields.reserve(count);
	for (short i = 0; i < count; i++)
	{
		wstring key = readUtf(dis, 256);
		wstring value = readUtf(dis, 4096);
		fields.emplace_back(std::move(key), std::move(value));
	}
}

void AuthPacket::write(DataOutputStream *dos)
{
	dos->writeByte(static_cast<byte>(stage));
	dos->writeShort(static_cast<short>(fields.size()));
	for (const auto &field : fields)
	{
		writeUtf(field.first, dos);
		writeUtf(field.second, dos);
	}
}

void AuthPacket::handle(PacketListener *listener)
{
	listener->handleAuth(shared_from_this());
}

int AuthPacket::getEstimatedSize()
{
	int size = 1 + 2;
	for (const auto &field : fields)
		size += 4 + static_cast<int>((field.first.length() + field.second.length()) * sizeof(wchar_t));
	return size;
}
