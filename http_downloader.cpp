// httpdownload.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "http_downloader.h"

http_downloader::http_downloader(size_t down_size /*= 1024 * 1024*/) 
	: _down_size(down_size)
{
	_buffer_queue.open(200);
}

http_downloader::~http_downloader(void)
{
	Stop();
	_buffer_queue.close();
}

bool http_downloader::download(const std::string &url, const std::string &file_name, bool is_cont /*= false*/,uint8_t id /*= 0*/)
{
	if (!m_bRunning) Run();

	FILE *f = fopen(file_name.c_str(), "wb+");
	if (f == NULL) return false;
	CRefObj<download_item> item = new download_item(f, url, 0, _down_size - 1, id);
	http::call3(item, make_functor(&http_downloader::http_handler, this, item.p));

	return true;
}

bool http_downloader::ThreadLoop()
{
	CRefObj<wbuffer_def> wbuffer;
	if (_buffer_queue.pop_timedwait(wbuffer, 200) == sem_queue<CRefObj<IBuffer>>::OK)
	{
		size_t wsize = 0;
		do
		{
			fseek(wbuffer->f, wbuffer->offset, SEEK_SET);
			int tmp_size = fwrite((char*)wbuffer->buffer->GetPointer() + wsize, sizeof(char), wbuffer->buffer->GetSize() - wsize, wbuffer->f);
			if (tmp_size < 0)
			{
				state_signal.emit(wbuffer->id, STATE_ERROR);
				return false;
			}
			wsize += tmp_size;
		} while (wsize < wbuffer->buffer->GetSize());

		prog_signal.emit(wbuffer->id, wbuffer->offset + wbuffer->buffer->GetSize(), wbuffer->total_len);
		if (wbuffer->offset + wbuffer->buffer->GetSize() == wbuffer->total_len)
		{
			state_signal.emit(wbuffer->id, STATE_COMPLETE);
			fclose(wbuffer->f);
		}
	}
	return true;
}

void http_downloader::http_handler(download_item *p)
{
	if (p->error_code() != http::error_ok) return;

	CRefObj<wbuffer_def> wbuffer = new wbuffer_def;
	wbuffer->buffer = p->recv_buffer;
	wbuffer->f = p->_f;
	wbuffer->offset = p->offset;
	wbuffer->id = p->get_id();
	wbuffer->total_len = p->total_len;
	_buffer_queue.push(wbuffer);

	if (!p->is_complete)
	{
		size_t s_pos = p->offset + p->recv_buffer->GetSize();
		size_t e_pos = s_pos + _down_size - 1;
		CRefObj<download_item> item = new download_item(p->_f, p->_url, s_pos, e_pos, p->get_id());
		http::call3(item, make_functor(&http_downloader::http_handler, this, item.p));
	}
}

class down_callback : public sigslot::has_slots<>
{
public:
	down_callback(http_downloader *downloader)
	{
		downloader->prog_signal.connect(this, &down_callback::prog_print);
		downloader->state_signal.connect(this, &down_callback::state_print);
	}
private:
	void prog_print(http_downloader::id_t id, int dl, int total)
	{
		printf("id:%d download:%d/%d\n", id, dl, total);
	}
	void state_print(http_downloader::id_t id, http_downloader::e_state state)
	{
		printf("id:%d state:%d\n", id, state);
	}
};

int main(int argc, char **argv)
{
	http::initialize();

	http_downloader down;
	down_callback callback(&down);
	down.download("http://download.oray.com/sunlogin/windows/SunloginRemote3.6.exe", "D:/test.exe");
	down.download("http://download.oray.com/sunlogin/windows/SunloginRemote3.6.exe", "D:/test2.exe", 2);
	down.download("http://download.oray.com/sunlogin/windows/SunloginRemote3.6.exe", "D:/test3.exe", 3);

	while (1)
	{
		Sleep(1000);
	}

	http::uninitialize();
    return 0;
}


http_downloader::download_item::download_item(FILE *f, const std::string &url, size_t range_begin, size_t range_end, uint8_t id) 
	:	_url(url),
		is_complete(false),
		_get_length(false),
		_r_flag(false),
		_last_r(false),
		_id(id),
		_f(f)
{
	char range_str[100] = { 0 };
	sprintf(range_str, "bytes=%d-%d", range_begin, range_end);
	extern_headers_["Range"] = range_str;
}

http_downloader::download_item::~download_item()
{

}

bool http_downloader::download_item::parse_hook(const char *data, size_t size)
{
	if (status_code_ != 200 && status_code_ != 206) return false;

	if (!_get_length)
	{
		std::string tmp_str(data, size > 1024 ? 1024 : size);
		std::string::size_type slen = tmp_str.find("Content-Range:");
		std::string::size_type elen = tmp_str.find("\r\n", slen);

		if (slen != std::string::npos && elen != std::string::npos)
		{
			std::string length_str = tmp_str.substr(slen, elen);

			size_t s_pos, e_pos, total_len;
			sscanf(length_str.c_str(), "Content-Range: bytes %d-%d/%d", &s_pos, &e_pos, &total_len);
			this->total_len = total_len;
			this->offset = s_pos;

			recv_buffer = g_pMemAlloctor->GetFreeBuffer(e_pos - s_pos + 1);

			if (e_pos == total_len - 1)
				is_complete = true;
		}

		if (_r_flag)
		{
			if (*data++ == '\n' && _last_r)
			{
				_get_length = true;
			}
			_r_flag = false;
		}

		if (!_get_length)
		{
			const char *test_data = data;
			size_t test_size = size;
			size_t tmp_size = 0;
			while (1)
			{
				tmp_size = 0;
				while (size > 1 && (*data++ != '\r' || *data != '\n'))
				{
					size--;
					tmp_size++;
				}

				if (size <= 1)
				{
					if (*data == '\r') _r_flag = true;
					if (tmp_size == 0) _last_r = true;
					break;
				}

				data++;
				size -= 2;
				if (tmp_size == 0) break;
			}

			//û���ҵ���Ӧ�壬ֱ�ӷ���
			if (size == 1 || tmp_size > 0)
				return true;

			//_get_length��־λ��true����ʶ��������ݶ�ֱ��д��buffer�
			_get_length = true;
		}
	}

	memcpy((char*)recv_buffer->GetPointer() + recv_buffer->GetSize(), data, size);
	recv_buffer->SetSize(recv_buffer->GetSize() + size);

	error_code(http::error_ok);

	return true;
}