#include <node.h>	/* 必須 */
#include <uv.h>		/* 非同期コールバックの為 */
#include <thread>	/* for std::this_thread::sleep_for() */
using namespace v8;	/* 必須 */

bool _bStop = false;
bool _bExec = false;

/* スレッド間情報 */
typedef struct my_struct
{
	int request;					/* 要求(処理待機秒) */
	int result;						/* 結果(待機した秒) */
	Persistent<Function> callback;	/* 非同期処理完了コールバック関数 */
};

void _stopping(uv_work_t* req) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	while (_bExec)	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		data->result++;
	}

}

void _stoped(uv_work_t* req, int status) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	Local<Value> argv[1] = { Number::New(data->result) };		/* コールバック用引数設定 */
	Local<Function> cb = Local<Function>::New(data->callback);	/* コールバック関数設定 */
	data->callback->Call(Context::GetCurrent()->Global(), 1, argv);
	/* 後処理 */
	delete data;
	delete req;
}

void _execute(uv_work_t* req) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */

	data->result = data->request;
	std::this_thread::sleep_for(std::chrono::milliseconds(data->request));
}

void _complete(uv_work_t* req, int ) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */

	Handle<Value> argv[1] = { Number::New(data->result) };		/* コールバック用引数設定 */
	data->callback->Call(Context::GetCurrent()->Global(), 1, argv);
	if (_bStop){
		/* 後処理 */
		delete data; data = NULL;
		delete req; req = NULL;
		_bExec = false;	/* 実行フラグ解除 */
		return;
	}
	uv_queue_work(uv_default_loop(), req, _execute, _complete);	/* 繰り返し */
}

//--------------------------------------------------------------------------------
//[外部]タイマー開始要求
//[引数]args[0]:	停止完了コールバック関数 */
//		args[1]:	インターバル(ms)
//[戻値] 0:開始要求受付,-1:タイマー実行中*/
//--------------------------------------------------------------------------------
Handle<Value> AsyncStart(const Arguments& args) {
	HandleScope scope;

	/* 引数チェック(引数の数) */
	if (args.Length() != 2){
		ThrowException(Exception::TypeError(
			String::New("Wrong number of arguments")));
	}
	/* 引数チェック(引数の型) */
	if (!args[0]->IsFunction() || !args[1]->IsNumber()) {
		ThrowException(Exception::TypeError(
			String::New("Wrong arguments")));
	}
	if (_bExec){
		return scope.Close(Number::New(-1));
	}
	else{
		/* 引数の情報を記憶 */
		my_struct* data = new my_struct;
		data->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
		data->request = args[1]->Int32Value();
		data->result = 0;
		_bExec = true;
		_bStop = false;

		/* バックグラウンドで実行 */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _execute, _complete);
		return scope.Close(Number::New(0));
	}
}
//--------------------------------------------------------------------------------
//[外部]タイマー停止要求
//[引数]args[0]:	停止完了コールバック関数 */
//[戻値] 0:停止要求受付,1:未実行時の停止要求受付*/
//--------------------------------------------------------------------------------
Handle<Value> AsyncStop(const Arguments& args) {
	HandleScope scope;

	/* 引数チェック(引数の数) */
	if (args.Length() != 1){
		ThrowException(Exception::TypeError(
			String::New("Wrong number of arguments")));
	}
	/* 未実行ならすぐにコールバックを返す */
	if (!_bExec){
		Local<Function> cb = Local<Function>::Cast(args[0]);
		Local<Value> argv[1] = { Number::New(0) };
		cb->Call(Context::GetCurrent()->Global(), 1, argv);
		return scope.Close(Number::New(1));
	}
	else{
		/* 引数の情報を記憶 */
		my_struct* data = new my_struct;
		data->callback = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
		data->result = 0;

		_bStop = true;	/* 停止要求 */

		/* バックグラウンドで実行 */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _stopping, _stoped);
		return scope.Close(Number::New(0));
	}
}

/* ここに外部から呼ばれる関数を書いていく */
/* 外部から呼ばれる名前と内部の関数名のひも付け */
void init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("start"),
		FunctionTemplate::New(AsyncStart)->GetFunction());
	exports->Set(String::NewSymbol("stop"),
		FunctionTemplate::New(AsyncStop)->GetFunction());}

/* モジュールがrequireされる時に呼ばれる */
NODE_MODULE(asyncTimer_v10, init)