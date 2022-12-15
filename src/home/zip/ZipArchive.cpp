#include <istream>
#include <unordered_map>

#include "minizip/mz.h"
#include "minizip/mz_os.h"
#include "minizip/mz_strm.h"
#include "minizip/mz_strm_buf.h"
#include "minizip/mz_strm_split.h"
#include "minizip/mz_zip.h"
#include "minizip/mz_zip_rw.h"

#include "ZipArchive.h"

using namespace HomeCompa;
using namespace Util;

namespace {

class InputStreamBuf
	: public std::streambuf
{
public:
	InputStreamBuf(void * handle, std::string_view name)
		: m_handle(handle)
		, m_name(name)
	{
		mz_zip_reader_set_pattern(m_handle, m_name.data(), 1);

		if (mz_zip_reader_goto_first_entry(m_handle) != MZ_OK)
			throw std::runtime_error("Cannot go to first entry");

		if (mz_zip_reader_entry_open(m_handle))
			throw std::runtime_error("Cannot open zip reader");

		setg(m_buf.get(), m_buf.get() + m_bufSize, m_buf.get() + m_bufSize);
	}

private: // std::streambuf
	std::streambuf * setbuf(char_type * /*s*/, std::streamsize /*n*/) override
	{
		assert(false && "implement me");
		return this;
	}

	pos_type seekoff(off_type /*off*/, std::ios_base::seekdir /*way*/, std::ios_base::openmode /*which*/) override
	{
		assert(false && "implement me");
		return 0;
	}

	pos_type seekpos(pos_type /*pos*/, std::ios_base::openmode /*which*/ = std::ios_base::in | std::ios_base::out) override
	{
		assert(false && "implement me");
		return 0;
	}

	int sync() override
	{
		assert(false && "implement me");
		return -1;
	}

	std::streamsize showmanyc() override
	{
		assert(false && "implement me");
		return 0;
	}

	int_type underflow() override
	{
		assert(false && "implement me");
		return std::char_traits<char>::eof();
	}

	int_type overflow(int_type /*c*/) override
	{
		assert(false && "implement me");
		return std::char_traits<char>::eof();
	}

	int_type uflow() override
	{
		assert(false && "implement me");
		return std::char_traits<char>::eof();
	}

	std::streamsize xsgetn(char_type * s, std::streamsize count) override
	{
		return mz_zip_reader_entry_read(m_handle, s, static_cast<int32_t>(count));
	}

	std::streamsize xsputn(const char_type * /*s*/, std::streamsize /*count*/) override
	{
		assert(false && "implement me");
		return 0;
	}

	int_type pbackfail(int_type /*c*/) override
	{
		assert(false && "implement me");
		return std::char_traits<char>::eof();
	}

private:
	void * const m_handle;
	const std::string m_name;
	static constexpr size_t m_bufSize { 1024ull * 32 };
	const std::unique_ptr<char[]> m_buf { new char[m_bufSize] };
};

class DecodedStream
	: public std::istream
{
public:
	DecodedStream(void * handle, std::string_view name)
		: std::istream(&m_buf)
		, m_buf(handle, name)
	{
	}

private:
	InputStreamBuf m_buf;
};

class BaseStream;
struct MzStreamWrapper : mz_stream
{
	BaseStream & baseStream;

	MzStreamWrapper(mz_stream_vtbl * vTbl, BaseStream & baseStream_)
		: mz_stream()
		, baseStream(baseStream_)
	{
		vtbl = vTbl;
		base = nullptr;
	}
};

class BaseStream
{
	NON_COPY_MOVABLE(BaseStream)

protected:
	BaseStream() = default;
	virtual ~BaseStream() = default;

	bool SetProperty(int32_t prop, int64_t value)
	{
		return set_prop(ToMzStream(), prop, value) == MZ_OK;
	}

	mz_stream * ToMzStream()
	{
		return &m_streamWrapper;
	}

private:
	virtual bool IsOpen() = 0;
	virtual int64_t Read(void * buf, int64_t size) = 0;
	virtual int64_t Write(const void * buf, int64_t size) = 0;
	virtual int64_t Tell() = 0;
	virtual bool Seek(int64_t offset, std::ios_base::seekdir origin) = 0;

private:
	static int32_t open(void * /*stream*/, const char * /*path*/ , int32_t /*mode*/ )
	{
		return {};
	}
	static int32_t is_open(void * stream)
	{
		return static_cast<MzStreamWrapper *>(stream)->baseStream.IsOpen() ? MZ_OK : MZ_OPEN_ERROR;
	}
	static int32_t read(void * stream, void * buf, int32_t size)
	{
		return static_cast<int32_t>(static_cast<MzStreamWrapper *>(stream)->baseStream.Read(buf, size));
	}
	static int32_t write(void * stream, const void * buf, int32_t size)
	{
		return static_cast<int32_t>(static_cast<MzStreamWrapper *>(stream)->baseStream.Write(buf, size));
	}
	static int64_t tell(void * stream)
	{
		return static_cast<MzStreamWrapper *>(stream)->baseStream.Tell();
	}
	static int32_t seek(void * stream, int64_t offset, int32_t origin)
	{
		return static_cast<MzStreamWrapper *>(stream)->baseStream.Seek(offset, origin == MZ_SEEK_SET ? std::ios_base::beg : origin == MZ_SEEK_END ? std::ios_base::end : origin == MZ_SEEK_CUR ? std::ios_base::cur : std::ios_base::beg) ? MZ_OK : MZ_SEEK_ERROR;
	}
	static int32_t close(void * /*stream*/)
	{
		return {};
	}
	static int32_t error(void * /*stream*/)
	{
		return {};
	}
	static void * create(void ** /*stream*/)
	{
		return {};
	}
	static void destroy(void ** /*stream*/)
	{
	}
	static int32_t get_prop(void * stream, int32_t prop, int64_t * value)
	{
		auto & self = static_cast<MzStreamWrapper *>(stream)->baseStream.m_properties;
		const auto it = self.find(prop);
		if (it == self.end())
			return MZ_EXIST_ERROR;

		*value = it->second;
		return MZ_OK;
	}
	static int32_t set_prop(void * stream, int32_t prop, int64_t value)
	{
		auto & self = static_cast<MzStreamWrapper *>(stream)->baseStream.m_properties;
		self.insert_or_assign(prop, value);
		return MZ_OK;
	}

	static mz_stream_vtbl CreateVTable()
	{
		return mz_stream_vtbl
		{
			  .open           = &BaseStream::open
			, .is_open        = &BaseStream::is_open
			, .read           = &BaseStream::read
			, .write          = &BaseStream::write
			, .tell           = &BaseStream::tell
			, .seek           = &BaseStream::seek
			, .close          = &BaseStream::close
			, .error          = &BaseStream::error
			, .create         = &BaseStream::create
			, .destroy        = &BaseStream::destroy
			, .get_prop_int64 = &BaseStream::get_prop
			, .set_prop_int64 = &BaseStream::set_prop
		};
	}

protected:
	void * m_handle { nullptr };

private:
	mz_stream_vtbl m_vTbl { CreateVTable() };
	MzStreamWrapper m_streamWrapper { &m_vTbl, *this };
	std::unordered_map<int32_t, int64_t> m_properties;
};

class InputStream
	: BaseStream
{
	NON_COPY_MOVABLE(InputStream)
public:
	explicit InputStream(std::istream & stream)
		: m_stream(stream)
	{
		if (true
			&& mz_zip_reader_create(&m_handle) != nullptr
			&& mz_zip_reader_open(m_handle, ToMzStream()) == MZ_OK
			)
			return;

		Cleanup();
		throw std::runtime_error("Cannot create stream");
	}

	~InputStream() override
	{
		Cleanup();
	}

	ZipEntries GetZipEntries() const
	{
		ZipEntries zipEntries;
		if (mz_zip_reader_goto_first_entry(m_handle) != MZ_OK)
			throw std::runtime_error("Cannot go to first entry");

		do
		{
			mz_zip_file * file_info = nullptr;
			if (mz_zip_reader_entry_get_info(m_handle, &file_info) != MZ_OK)
				throw std::runtime_error("Cannot get entry info");

			zipEntries.emplace_back(file_info->filename, file_info->uncompressed_size, file_info->compressed_size);

			const auto res = mz_zip_reader_goto_next_entry(m_handle);
			if (res == MZ_END_OF_LIST)
				break;

			if (res != MZ_OK)
				throw std::runtime_error("Cannot go to next entry");
		}
		while (true);

		return zipEntries;
	}

	std::istream & GetDecodedStream(std::string_view name)
	{
		PropagateConstPtr<DecodedStream>(std::make_unique<DecodedStream>(m_handle, name)).swap(m_decodedStream);
		return *m_decodedStream;
	}

	void Cleanup() noexcept
	{
		if (!m_handle)
			return;

		mz_zip_reader_close(m_handle);
		mz_zip_reader_delete(&m_handle);
	}

private: // BaseStream
	bool IsOpen() override
	{
		return m_stream.good() && !m_stream.eof();
	}

	int64_t Read(void * buf, int64_t size) override
	{
		m_stream.read(static_cast<char *>(buf), size);
		return m_stream.gcount();
	}

	int64_t Write(const void * /*buf*/, int64_t /*size*/) override
	{
		throw std::runtime_error("Cannot write to read only stream");
	}

	int64_t Tell() override
	{
		return m_stream.tellg();
	}

	bool Seek(int64_t offset, std::ios_base::seekdir origin) override
	{
		m_stream.seekg(offset, origin);
		return m_stream.good();
	}

private:
	std::istream & m_stream;
	PropagateConstPtr<DecodedStream> m_decodedStream { std::unique_ptr<DecodedStream>() };
};

class OutputStream
	: BaseStream
{
	NON_COPY_MOVABLE(OutputStream)
public:
	explicit OutputStream(std::ostream & stream)
		: m_stream(stream)
	{
		if (!mz_zip_writer_create(&m_handle))
			throw std::runtime_error("Cannot create stream");

		mz_zip_writer_set_password(m_handle, nullptr);
		mz_zip_writer_set_aes(m_handle, 0);
		mz_zip_writer_set_compress_method(m_handle, MZ_COMPRESS_METHOD_DEFLATE);
		mz_zip_writer_set_compress_level(m_handle, MZ_COMPRESS_LEVEL_BEST);
		mz_zip_writer_set_follow_links(m_handle, 0);
		mz_zip_writer_set_store_links(m_handle, 0);
		mz_zip_writer_set_zip_cd(m_handle, 0);

		if (mz_zip_writer_open(m_handle, ToMzStream(), 0) == MZ_OK)
			return;

		Cleanup();
	}

	~OutputStream() override
	{
		Cleanup();
	}

	int64_t Write(std::string_view name, std::istream & stream)
	{
		const std::string fileName(name);
		mz_zip_file info{};
		info.filename = fileName.data();
		info.version_madeby = MZ_VERSION_MADEBY;
		info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
		info.flag = MZ_ZIP_FLAG_UTF8;
		info.accessed_date = info.creation_date = info.modified_date = mz_os_ms_time();

		const uint8_t srcSys = MZ_HOST_SYSTEM(info.version_madeby);
		const uint32_t srcAttribute = 0x20;
		if (srcSys == MZ_HOST_SYSTEM_MSDOS || srcSys == MZ_HOST_SYSTEM_WINDOWS_NTFS)
		{
			info.external_fa = srcAttribute;
		}
		else
		{
			uint32_t targetAttribute = 0;
			if (mz_zip_attrib_convert(srcSys, srcAttribute, MZ_HOST_SYSTEM_MSDOS, &targetAttribute) == MZ_OK)
				info.external_fa = targetAttribute;
			info.external_fa |= (srcAttribute << 16);
		}

		SetProperty(MZ_STREAM_PROP_DISK_NUMBER, 0);
		if (mz_zip_writer_entry_open(m_handle, &info) != MZ_OK)
			throw std::runtime_error("Cannot open writer entry");

		static constexpr auto bufferSize = 16 * 1024ull;
		const std::unique_ptr<char[]> buffer(new char[bufferSize]);
		while(!stream.eof())
		{
			const auto size = static_cast<int32_t>(stream.read(buffer.get(), bufferSize).gcount());
			[[maybe_unused]] const auto written = mz_zip_writer_entry_write(m_handle, buffer.get(), size);
			assert(size == written);
		}

		return 0;
	}

private: // BaseStream
	bool IsOpen() override
	{
		return m_stream.good();
	}

	int64_t Read(void * /*buf*/, int64_t /*size*/) override
	{
		throw std::runtime_error("Cannot read from write only stream");
	}

	int64_t Write(const void * buf, int64_t size) override
	{
		m_stream.write(static_cast<const char *>(buf), size);
		return size;
	}

	int64_t Tell() override
	{
		return m_stream.tellp();
	}

	bool Seek(int64_t offset, std::ios_base::seekdir origin) override
	{
		m_stream.seekp(offset, origin);
		return m_stream.good();
	}

private:
	void Cleanup() noexcept
	{
		if (!m_handle)
			return;

		mz_zip_writer_close(m_handle);
		mz_zip_writer_delete(&m_handle);
	}

private:
	std::ostream & m_stream;
};

}

class ZipArchive::Impl
{
public:
	explicit Impl(std::istream & stream)
		: m_inputStream(stream)
	{
	}

	explicit Impl(std::ostream & stream)
		: m_outputStream(stream)
	{
	}

	ZipEntries GetZipEntries() const
	{
		if (!m_inputStream)
			throw std::runtime_error("Cannot get entries list from output stream");

		return m_inputStream->GetZipEntries();
	}

	std::istream & GetDecodedStream(std::string_view name)
	{
		return m_inputStream->GetDecodedStream(name);
	}

	int64_t Write(std::string_view name, std::istream & stream)
	{
		return m_outputStream->Write(name, stream);
	}

private:
	PropagateConstPtr<InputStream> m_inputStream { std::unique_ptr<InputStream>() };
	PropagateConstPtr<OutputStream> m_outputStream { std::unique_ptr<OutputStream>() };
};

ZipArchive::ZipArchive(std::istream & stream)
	: m_impl(stream)
{
}

ZipArchive::ZipArchive(std::ostream & stream)
	: m_impl(stream)
{
}

ZipEntries ZipArchive::GetZipEntries() const
{
	return m_impl->GetZipEntries();
}

std::istream & ZipArchive::Read(std::string_view name)
{
	return m_impl->GetDecodedStream(name);
}

int64_t ZipArchive::Write(std::string_view name, std::istream & stream)
{
	return m_impl->Write(name, stream);
}

ZipArchive::~ZipArchive() = default;
