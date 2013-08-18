#include "ofxGit.h"

OFX_GIT_BEGIN_NAMESPACE

class Remote : public HasTransferProgressCallback
{
public:
	
	class Connection
	{
	public:
		
		Connection(const Remote& remote_, git_direction direction)
		{
			remote = remote_.getRemote();
			
			int err = git_remote_connect(remote, direction);
			checkError(err);
		}
		
		~Connection()
		{
			if (remote && git_remote_connected(remote))
			{
				git_remote_disconnect(remote);
				remote = NULL;
			}
		}
		
	private:
		
		git_remote *remote;
		
		Connection(const Connection&);
		Connection& operator=(const Connection&);
	};
	
	Remote() : remote_url(""), remote(NULL) {}
	~Remote() { close(); }
	
	bool open(git_repository* local, const string& remote_name, const string& remote_url)
	{
		int err;
		
		err = git_remote_load(&remote, local, remote_name.c_str());
		if (!checkError(err)) return false;
		
		git_remote_check_cert(remote, false);
		
		return true;
	}
	
	void close()
	{
		if (remote)
		{
			git_remote_free(remote);
			remote = NULL;
		}
	}
	
	string getName() const
	{
		assert(remote);
		return git_remote_name(remote);
	}
	
	string getUrl() const
	{
		assert(remote);
		return git_remote_url(remote);
	}
	
	void list() const
	{
		if (!isConnected()) return false;
		
		int err;
		
		err = git_remote_ls(remote, git_headlist_cb, (void*)this);
		if (!checkError(err)) return false;
		
		return true;
	}
	
	bool isConnected() const
	{
		return remote && git_remote_connected(remote);
	}
	
	git_remote* getRemote() const { return remote; }
	
protected:
	
	string remote_url;
	git_remote *remote;
	
	static int git_headlist_cb(git_remote_head *rhead, void *payload)
	{
		cout << rhead->local << " " << rhead->name << endl;
		return 0;
	}
};

OFX_GIT_END_NAMESPACE