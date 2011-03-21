#include <assert.h>
#include "erl_nif.h"
#include "hiredis.h"
#include "async.h"
#include "adapters/libevent.h"

typedef struct {
    ErlNifThreadOpts*   opts;
    ErlNifTid           qthread;
    struct event_base*         event_base;
    redisAsyncContext*  context;
    ERL_NIF_TERM        atom_ok;
} state_t;

void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;
    if (reply == NULL) return;
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);
}

static void*
thr_main(void* obj) {
    state_t* state = (state_t*) obj;
    struct event_base *base = event_base_new();
    state->event_base = base; 
    event_base_dispatch(base);
    return 0;
}

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info) {
    state_t* state = (state_t*) enif_alloc(sizeof(state_t));
    if(state == NULL) return -1;

    state->opts = enif_thread_opts_create("thread_opts");
    if(enif_thread_create("", &(state->qthread), thr_main, state, state->opts) != 0) {
        return 1;
    }

    state->atom_ok = enif_make_atom(env, "ok");

    *priv = (void*) state;

    return 0;
}

static void
unload(ErlNifEnv* env, void* priv) {
    state_t* state = (state_t*) priv;
    void* resp;
    
    enif_thread_join(state->qthread, &resp);

    enif_thread_opts_destroy(state->opts);
    enif_free(state);
}

void connectCallback(const redisAsyncContext *c) {
    ((void)c);
    printf("connected...\n");
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
    }
    printf("disconnected...\n");
}

static ERL_NIF_TERM
redis_connect(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    state_t* state = (state_t*) enif_priv_data(env);
    char ip[1024];
    int port;

    if(argc != 2) {
        return enif_make_badarg(env);
    }

    if(!enif_get_string(env, argv[0], ip, 1024, ERL_NIF_LATIN1)) {
        return enif_make_badarg(env);
    }

    if(!enif_get_int(env, argv[1], &port)) {
        return enif_make_badarg(env);
    }

    redisAsyncContext *c = redisAsyncConnect(ip, port);
    if (c->err) {
        // Let *c leak for now...
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    state->context = c;

    redisLibeventAttach(c, state->event_base);
    redisAsyncSetConnectCallback(c, connectCallback);
    redisAsyncSetDisconnectCallback(c, disconnectCallback);

    return state->atom_ok; 
}

static ERL_NIF_TERM
command(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    state_t* state = (state_t*) enif_priv_data(env);
    //ErlNifPid* pid = (ErlNifPid*) enif_alloc(sizeof(ErlNifPid));

    /*if(!enif_get_local_pid(env, argv[0], pid)) {
        return enif_make_badarg(env);
    }*/

    return state->atom_ok;
}

static ErlNifFunc nif_funcs[] = {
    {"connect", 2, redis_connect},
    //{"connect_unix", 1, connectUnix},
    {"command", 1, command}
};

ERL_NIF_INIT(supredis, nif_funcs, load, unload, NULL, NULL);

