#pragma once

class QIODevice;

class Stream
{
public:
	virtual ~Stream()              = default;
	virtual QIODevice& GetStream() = 0;
};
