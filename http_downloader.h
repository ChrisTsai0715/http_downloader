#ifndef HTTP_DOWNLOADER_CAIRUI_20181013
#define HTTP_DOWNLOADER_CAIRUI_20181013

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <stdexcept>
#include <map>

#include "reference/IReference.h"
#include "thread/BaseThread.h"
#include "phstream/semqueue.h"
#include "http_call/http_call.h"
#include "http_call/http_call_class.h"
#include "memalloc/imemalloc.h"
#include "memalloc/mallocins.h"
#include "talk/base/sigslot.h"

class download_item;

class http_downloader : public CReference,
						public CBaseThread
{
public:
	typedef uint8_t id_t;

	typedef enum
	{
		STATE_INIT = 0,
		STATE_PROCESS,
		STATE_COMPLETE,
		STATE_PAUSE,
		STATE_CANCEL,
		STATE_ERROR,
	}e_state;

	//一次range的大小，默认100KB
	http_downloader(size_t down_size = 1024 * 1024);
	virtual ~http_downloader(void);

public:
	//************************************
	// Method:    download
	// Returns:   bool 返回状态
	// Parameter: const std::string & url	下载的url
	// Parameter: const std::string & file_name 保存的文件名
	// Parameter: bool is_cont	是否为断点续传
	// Parameter: uint8_t id 下载id。作用为同时下载多个文件时，区分每个回调的序号
	//************************************
	virtual bool download(const std::string &url, const std::string &file_name, bool is_cont = false, uint8_t id = 0);
	virtual bool pause(id_t id);
	virtual bool resume(id_t id);
	virtual bool cancel(id_t id);
	virtual bool ThreadLoop();

public:
	//进度信号, 参数为id，已当前下载大小，文件总大小
	sigslot::signal3<id_t, int, int> prog_signal;
	//状态信号, 参数为id，当前状态
	sigslot::signal2<id_t, e_state> state_signal;

protected:
	class download_item : public http::call_class
	{
	public:
		download_item(const std::string &url, size_t range_begin, size_t range_end, uint8_t id);

		virtual ~download_item();

		virtual const char *url() { return _url.c_str();}
		virtual const http::call_method method() { return http::get_method; }
		virtual bool parse_hook(const char *data, size_t size);
		http_downloader::id_t get_id() const { return _id; }

	public:
		CRefObj<IBuffer> recv_buffer;
		size_t total_len;
		size_t offset;
		std::string _url;
		bool is_complete;

	private:
		bool _get_length;
		bool _r_flag;
		bool _last_r;
		http_downloader::id_t _id;
	};

private:
	typedef struct _wbuffer_def : public CReference
	{
		CRefObj<IBuffer> buffer;
		FILE *f;
		size_t offset;
		id_t id;
		size_t total_len;
	}wbuffer_def;

	typedef struct  
	{
		std::string url;
		std::string file_name;
		FILE *f;
		e_state state;
		size_t offset;
	}down_info;

private:
	void http_handler(download_item *p);

private:
	sem_queue<CRefObj<wbuffer_def>> _buffer_queue;
	size_t _down_size;
	std::map<id_t, down_info> _down_infos;
	CMutexLock _mutex;

	friend class download_item;
};


#endif // !HTTP_DOWNLOADER_CAIRUI_20181013
