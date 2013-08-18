#pragma once

#include "ofMain.h"

#include "git2.h"

#define OFX_GIT_BEGIN_NAMESPACE namespace ofxGit {
#define OFX_GIT_END_NAMESPACE }

OFX_GIT_BEGIN_NAMESPACE

inline bool checkError(int error)
{
	if (error != 0)
	{
		const git_error *err = giterr_last();
		if (err) ofLogError("ofxGit", "%s\n", err->message);
		else ofLogError("ofxGit", "ERROR %d: no detailed info\n", error);
	}

	return true;
}

class HasCheckoutProgressCallback
{
public:

	static void git_checkout_progress_cb(const char *path, size_t cur, size_t tot, void *payload)
	{
		HasCheckoutProgressCallback *self = (HasCheckoutProgressCallback*)payload;
		float v = (float)cur / tot;
		ofLogVerbose("ofxGit", "checkout_progress: %i %%", (int)(100 * v));
		fflush(stdout);
	}
};

class HasTransferProgressCallback
{
public:

	static int git_transfer_progress_callback(const git_transfer_progress *stats, void *payload)
	{
		HasTransferProgressCallback *self = (HasTransferProgressCallback*)payload;

		float v = (float)stats->received_objects / stats->total_objects;
		ofLogVerbose("ofxGit", "transfer_progress: %i %%", (int)(100 * v));
		fflush(stdout);
		return 0;
	}
};

class GitTask : public ofThread
{
public:

	GitTask(bool blocking) : blocking(blocking), ofThread() {}

	void exec()
	{
		// TODO: more better block/nonblock switch mechanism

		if (blocking)
		{
			task();
			done();
			delete this;
		}
		else
		{
			startThread();
		}
	}

	virtual void task() {}
	virtual void done() {}

protected:

	bool blocking;

	void threadedFunction()
	{
		task();
		done();
		delete this;
	}
};

class OID
{
public:

	OID(const git_oid* oid) : oid(oid)
	{
		git_oid_fmt(text, oid);
		text[40] = '\0';
	}

	const char* str() const { return text; }

	const git_oid* get() const { return oid; }
	operator const git_oid*() const { return oid; }

private:
	const git_oid* oid;
	char text[41];
};

inline ostream& operator<<(ostream& ost, const OID& oid) { ost << oid.str(); return ost; }

class Object
{
public:

	Object(git_repository *repo, const git_oid *oid)
	{
		int err;

		err = git_object_lookup(&object, repo, oid, GIT_OBJ_ANY);
		if (!checkError(err)) object = NULL;
	}

	~Object()
	{
		if (object) git_object_free(object), object = NULL;
	}

	git_object* get() const { return object; }
	operator git_object*() const { return object; }

protected:

	git_object *object;
};

class Commit
{
public:

	Commit(git_repository *repo, const git_oid *oid)
	{
		int err;

		err = git_commit_lookup(&commit, repo, oid);
		if (!checkError(err)) commit = NULL;
	}

	~Commit()
	{
		if (commit) git_commit_free(commit), commit = NULL;
	}

	const git_commit* get() const { return commit; }
	operator const git_commit*() const { return commit; }

protected:

	git_commit *commit;
};

class Reference
{
public:

	Reference(git_repository *repo) : repo(repo), ref(NULL) {}
	Reference(git_repository *repo, git_reference *ref) : repo(repo), ref(ref) {}

	virtual ~Reference()
	{
		close();
		repo = NULL;
	}

	void close()
	{
		if (ref) git_reference_free(ref), ref = NULL;
	}

	OID getOID() const
	{
		assert(ref);
		return git_reference_target(ref);
	}

	bool isValid() const { return ref != NULL; }

	git_reference* get() const { return ref; }
	operator git_reference*() const { return ref; }

protected:

	git_repository *repo;
	git_reference *ref;

private:

	Reference();
	Reference(const Reference&);
	Reference& operator=(const Reference&);
};

class Branch : public Reference
{
public:

	typedef ofPtr<Branch> Ptr;

	Branch(git_repository *repo, const string& branch_name) : Reference(repo), branch_name(branch_name)
	{
		int err;

		if (git_branch_lookup(&ref, repo, branch_name.c_str(), GIT_BRANCH_LOCAL) == 0)
		{
			branch_type = GIT_BRANCH_LOCAL;
		}
		else if (git_branch_lookup(&ref, repo, branch_name.c_str(), GIT_BRANCH_REMOTE) == 0)
		{
			branch_type = GIT_BRANCH_REMOTE;
		}
		else
		{
			ref = NULL;
		}
	}

	Ptr fork(const string& new_branch_name)
	{
		assert(isValid());

		int err;

		Branch *new_branch = new Branch(repo, new_branch_name);
		new_branch->branch_type = GIT_BRANCH_LOCAL;

		Commit commit(repo, getOID());

		err = git_branch_create(&new_branch->ref, repo, new_branch_name.c_str(), commit, 0);
		if (!checkError(err)) goto cleanup;

		return Ptr(new_branch);

	cleanup:

		delete new_branch;
		return Ptr();
	}

	bool makeHead()
	{
		assert(isValid());

		int err;
		if (branch_type != GIT_BRANCH_LOCAL) return false;

		err = git_repository_set_head(repo, git_reference_name(get()));
		if (!checkError(err)) return false;

		Object target(repo, getOID());
		git_reset(repo, target, GIT_RESET_HARD);

		return true;
	}

	bool remove()
	{
		assert(isValid());

		int err = git_branch_delete(get());
		if (!checkError(err)) return false;

		ref = NULL;
		return true;
	}

	string getName()
	{
		assert(isValid());

		int err;
		const char *out;
		err = git_branch_name(&out, ref);
		if (!checkError(err)) return "";

		return out;
	}

	// TODO: Rename
	
private:

	string branch_name;
	git_branch_t branch_type;

	Branch();
	Branch(const Branch&);
	Branch& operator=(const Branch&);
};

class Repository : public HasCheckoutProgressCallback, public HasTransferProgressCallback
{
public:

	ofEvent<ofEventArgs> complete;

	Repository() : repo(NULL)
	{
		git_threads_init();
	}

	~Repository() { close(); }

	bool open(const string& local_path_)
	{
		local_path = ofToDataPath(local_path_);

		if (!ofDirectory(local_path).exists())
			return false;

		close();

		int err = git_repository_open_ext(&repo, local_path.c_str(), 0, NULL);
		if (err == 0) return true;
		else false;
	}

	bool clone(const string& remote_url, bool blocking = true)
	{
		close();
		(new CloneTask(this, remote_url, local_path, blocking))->exec();
	}

	void close() { if (repo) git_repository_free(repo), repo = NULL; }

	vector<string> list() const
	{
		vector<string> result;
		int err;

		err = git_branch_foreach(repo, GIT_BRANCH_LOCAL, git_branch_foreach_cb, &result);
		if (!checkError(err)) return vector<string>();

		err = git_branch_foreach(repo, GIT_BRANCH_REMOTE, git_branch_foreach_cb, &result);
		if (!checkError(err)) return vector<string>();

		return result;
	}

	void dump() const
	{
		vector<string> names = list();
		for (int i = 0; i < names.size(); i++)
			cout << names[i] << endl;
	}

	Branch::Ptr getBranch(const string& branch_name)
	{
		Branch *o = new Branch(repo, branch_name);

		if (!o->isValid())
		{
			delete o;
			return Branch::Ptr();
		}

		return Branch::Ptr(o);
	}

	string getCurrentBranchName() const
	{
		int err;

		git_reference *ref;
		err = git_repository_head(&ref, repo);
		if (!checkError(err)) return "";

		const char *name;
		err = git_branch_name(&name, ref);
		if (!checkError(err)) return "";

		return name;
	}

	git_repository* get() const { return repo; }
	operator git_repository*() const { return repo; }

	protected:

	class CloneTask : public GitTask
	{
	public:

		CloneTask(Repository *repo, const string& remote_url, const string& local_path, bool blocking)
			: GitTask(blocking),
			local_path(local_path),
			remote_url(remote_url),
			repo(repo) {}

		void task()
		{
			git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
			git_checkout_opts checkout_opts = GIT_CHECKOUT_OPTS_INIT;

			checkout_opts.progress_cb = HasCheckoutProgressCallback::git_checkout_progress_cb;
			checkout_opts.progress_payload = repo;
			checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

			clone_opts.fetch_progress_cb = HasTransferProgressCallback::git_transfer_progress_callback;
			clone_opts.fetch_progress_payload = repo;
			clone_opts.transport_flags = GIT_TRANSPORTFLAGS_NO_CHECK_CERT;
			clone_opts.checkout_opts = checkout_opts;

			int err = git_clone(&repo->repo, remote_url.c_str(), local_path.c_str(), &clone_opts);

			checkError(err);
		}

		void done()
		{
			static ofEventArgs e;
			ofNotifyEvent(repo->complete, e);

			ofLogVerbose("ofxGit", "clone done");
		}

		string remote_url;
		string local_path;

		Repository *repo;
	};

	string local_path;
	git_repository *repo;

	static int git_branch_foreach_cb(const char *branch_name,
									 git_branch_t branch_type,
									 void *payload)
	{
		vector<string> *result = (vector<string>*)payload;
		result->push_back(branch_name);
		return 0;
	}
};

OFX_GIT_END_NAMESPACE