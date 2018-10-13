#ifndef HTTP_DOWNLOADER_CAIRUI_20181013
#define HTTP_DOWNLOADER_CAIRUI_20181013

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <stdexcept>

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
		STATE_BEGIN = 0,
		STATE_COMPLETE,
		STATE_ERROR,
	}e_state;

	//һ��range�Ĵ�С��Ĭ��100KB
	http_downloader(size_t down_size = 1024 * 1024);
	virtual ~http_downloader(void);

public:
	//************************************
	// Method:    download
	// Returns:   bool ����״̬
	// Parameter: const std::string & url	���ص�url
	// Parameter: const std::string & file_name ������ļ���
	// Parameter: bool is_cont	�Ƿ�Ϊ�ϵ�����
	// Parameter: uint8_t id ����id������Ϊͬʱ���ض���ļ�ʱ������ÿ���ص������
	//************************************
	virtual bool download(const std::string &url, const std::string &file_name, bool is_cont = false, uint8_t id = 0);
	virtual bool ThreadLoop();

public:
	//�����ź�, ����Ϊid���ѵ�ǰ���ش�С���ļ��ܴ�С
	sigslot::signal3<id_t, int, int> prog_signal;
	//״̬�ź�, ����Ϊid����ǰ״̬
	sigslot::signal2<id_t, e_state> state_signal;

protected:
	class download_item : public http::call_class
	{
	public:
		download_item(FILE *f, const std::string &url, size_t range_begin, size_t range_end, uint8_t id);

		virtual ~download_item();

		virtual const char *url() { return _url.c_str();}
		virtual const http::call_method method() { return http::get_method; }
		virtual bool parse_hook(const char *data, size_t size);
		http_downloader::id_t get_id() const { return _id; }

	public:
		CRefObj<IBuffer> recv_buffer;
		FILE *_f;
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

private:
	sem_queue<CRefObj<wbuffer_def>> _buffer_queue;
	size_t _down_size;

private:
	void http_handler(download_item *p);
};


#endif // !HTTP_DOWNLOADER_CAIRUI_20181013
