#include <assert.h>
#include "erl_nif.h"

typedef struct {
    ErlNifThreadOpts*   opts;
    ErlNifTid           qthread;
    ERL_NIF_TERM        atom_ok;
} state_t;

static void*
thr_main(void* obj) {
    state_t* state = (state_t*) obj;
    ErlNifEnv* env = enif_alloc_env();
    ErlNifPid* pid;
    ERL_NIF_TERM msg;

    return NULL;
}


static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info) {
    state_t* state = (state_t*) enif_alloc(sizeof(state_t));
    if(state == NULL) return -1;

    state->opts = enif_thread_opts_create("thread_opts");
    if(enif_thread_create("", &(state->qthread), thr_main, state, state->opts) != 0) {
        goto error;
    }

    state->atom_ok = enif_make_atom(env, "ok");

    *priv = (void*) state;

    return 0;

error:
    return -1;
}

static void
unload(ErlNifEnv* env, void* priv) {
    state_t* state = (state_t*) priv;
    void* resp;
    
    enif_thread_join(state->qthread, &resp);

    enif_thread_opts_destroy(state->opts);
    enif_free(state);
}

static ERL_NIF_TERM
connect(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ErlNifBinary ip;
    int port;

    if(argc != 2) {
        return enif_make_badarg(env);
    }

    if(!enif_inspect_binary(env, argv[0], &ip) {
        return enif_make_badarg(env);
    }

    if(!enif_get_int(env, argv[1], &port)) {
        return enif_make_badarg(env);
    }

    redisAsyncContext *c = redisAsyncConnect(ip, port);
    if (c->err) {
        printf("Error: %s\n", c->errstr);
        // handle error
    }
    
}

static ERL_NIF_TERM
command(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    state_t* state = (state_t*) enif_priv_data(env);
    ErlNifPid* pid = (ErlNifPid*) enif_alloc(sizeof(ErlNifPid));

    if(!enif_get_local_pid(env, argv[0], pid)) {
        return enif_make_badarg(env);
    }

    return state->atom_ok;
}

static ErlNifFunc nif_funcs[] = {
    {"connect", 2, connect},
    //{"connect_unix", 1, connectUnix},
    {"command", 1, command}
};

ERL_NIF_INIT(supredis, nif_funcs, load, unload, NULL, NULL);

