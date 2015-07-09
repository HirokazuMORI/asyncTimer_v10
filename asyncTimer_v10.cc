#include <node.h>	/* �K�{ */
#include <uv.h>		/* �񓯊��R�[���o�b�N�̈� */
#include <thread>	/* for std::this_thread::sleep_for() */
using namespace v8;	/* �K�{ */

bool _bStop = false;
bool _bExec = false;

/* �X���b�h�ԏ�� */
typedef struct my_struct
{
	int request;					/* �v��(�����ҋ@�b) */
	int result;						/* ����(�ҋ@�����b) */
	Persistent<Function> callback;	/* �񓯊����������R�[���o�b�N�֐� */
};

void _stopping(uv_work_t* req) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	while (_bExec)	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		data->result++;
	}

}

void _stoped(uv_work_t* req, int status) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	Local<Value> argv[1] = { Number::New(data->result) };		/* �R�[���o�b�N�p�����ݒ� */
	Local<Function> cb = Local<Function>::New(data->callback);	/* �R�[���o�b�N�֐��ݒ� */
	data->callback->Call(Context::GetCurrent()->Global(), 1, argv);
	/* �㏈�� */
	delete data;
	delete req;
}

void _execute(uv_work_t* req) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */

	data->result = data->request;
	std::this_thread::sleep_for(std::chrono::milliseconds(data->request));
}

void _complete(uv_work_t* req, int ) {

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */

	Handle<Value> argv[1] = { Number::New(data->result) };		/* �R�[���o�b�N�p�����ݒ� */
	data->callback->Call(Context::GetCurrent()->Global(), 1, argv);
	if (_bStop){
		/* �㏈�� */
		delete data; data = NULL;
		delete req; req = NULL;
		_bExec = false;	/* ���s�t���O���� */
		return;
	}
	uv_queue_work(uv_default_loop(), req, _execute, _complete);	/* �J��Ԃ� */
}

//--------------------------------------------------------------------------------
//[�O��]�^�C�}�[�J�n�v��
//[����]args[0]:	��~�����R�[���o�b�N�֐� */
//		args[1]:	�C���^�[�o��(ms)
//[�ߒl] 0:�J�n�v����t,-1:�^�C�}�[���s��*/
//--------------------------------------------------------------------------------
Handle<Value> AsyncStart(const Arguments& args) {
	HandleScope scope;

	/* �����`�F�b�N(�����̐�) */
	if (args.Length() != 2){
		ThrowException(Exception::TypeError(
			String::New("Wrong number of arguments")));
	}
	/* �����`�F�b�N(�����̌^) */
	if (!args[0]->IsFunction() || !args[1]->IsNumber()) {
		ThrowException(Exception::TypeError(
			String::New("Wrong arguments")));
	}
	if (_bExec){
		return scope.Close(Number::New(-1));
	}
	else{
		/* �����̏����L�� */
		my_struct* data = new my_struct;
		data->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
		data->request = args[1]->Int32Value();
		data->result = 0;
		_bExec = true;
		_bStop = false;

		/* �o�b�N�O���E���h�Ŏ��s */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _execute, _complete);
		return scope.Close(Number::New(0));
	}
}
//--------------------------------------------------------------------------------
//[�O��]�^�C�}�[��~�v��
//[����]args[0]:	��~�����R�[���o�b�N�֐� */
//[�ߒl] 0:��~�v����t,1:�����s���̒�~�v����t*/
//--------------------------------------------------------------------------------
Handle<Value> AsyncStop(const Arguments& args) {
	HandleScope scope;

	/* �����`�F�b�N(�����̐�) */
	if (args.Length() != 1){
		ThrowException(Exception::TypeError(
			String::New("Wrong number of arguments")));
	}
	/* �����s�Ȃ炷���ɃR�[���o�b�N��Ԃ� */
	if (!_bExec){
		Local<Function> cb = Local<Function>::Cast(args[0]);
		Local<Value> argv[1] = { Number::New(0) };
		cb->Call(Context::GetCurrent()->Global(), 1, argv);
		return scope.Close(Number::New(1));
	}
	else{
		/* �����̏����L�� */
		my_struct* data = new my_struct;
		data->callback = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
		data->result = 0;

		_bStop = true;	/* ��~�v�� */

		/* �o�b�N�O���E���h�Ŏ��s */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _stopping, _stoped);
		return scope.Close(Number::New(0));
	}
}

/* �����ɊO������Ă΂��֐��������Ă��� */
/* �O������Ă΂�閼�O�Ɠ����̊֐����̂Ђ��t�� */
void init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("start"),
		FunctionTemplate::New(AsyncStart)->GetFunction());
	exports->Set(String::NewSymbol("stop"),
		FunctionTemplate::New(AsyncStop)->GetFunction());}

/* ���W���[����require����鎞�ɌĂ΂�� */
NODE_MODULE(asyncTimer_v10, init)