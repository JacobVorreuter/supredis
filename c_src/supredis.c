#include <assert.h>
#include <string.h>
#include "erl_nif.h"
#include "hiredis.h"
#include "async.h"
#include "adapters/libev.h"

static ErlNifResourceType* async_context_type;

typedef struct {
    ErlNifThreadOpts*   opts;
    ErlNifTid           qthread;
    ERL_NIF_TERM        atom_ok;
} state_t;

static void*
thr_main(void* obj) {
    ev_loop(EV_DEFAULT_ 0);
    return 0;
}

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM load_info) {
    async_context_type = enif_open_resource_type(env, NULL, "async_context_resource", NULL, ERL_NIF_RT_CREATE, NULL);
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

void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    printf("recv'd response\n");
    redisReply *reply = r;
    if (reply == NULL) return;
    ErlNifPid* pid = (ErlNifPid*) privdata;
    ErlNifEnv* env = enif_alloc_env();
    ERL_NIF_TERM msg;
    ERL_NIF_TERM str;
    ERL_NIF_TERM atom;
    ERL_NIF_TERM pid1;
    str = enif_make_string(env, reply->str, ERL_NIF_LATIN1);
    atom = enif_make_atom(env, "supredis");
    pid1 = enif_make_pid(env, pid);
    msg = enif_make_tuple3(env, atom, pid1, str);
    enif_send(NULL, pid, env, msg);
    enif_free(pid);
    enif_clear_env(env);
}

static ERL_NIF_TERM
redis_connect(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    ERL_NIF_TERM resource;
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

    redisAsyncContext **c = enif_alloc_resource(async_context_type, sizeof(redisAsyncContext));

    *c = redisAsyncConnect(ip, port);
    if ((*c)->err) {
        // Let *c leak for now...
        printf("Error: %s\n", (*c)->errstr);
        return 1;
    }

    redisLibevAttach(EV_DEFAULT_ *c);
    redisAsyncSetConnectCallback(*c, connectCallback);
    redisAsyncSetDisconnectCallback(*c, disconnectCallback); 

    resource = enif_make_resource(env, c);
    enif_release_resource(c);

    (void)fprintf(stderr, "resource: p=%p/%p\n", *c, c);
    return resource;
}

static ERL_NIF_TERM
async_command(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    state_t* state = (state_t*) enif_priv_data(env);
    redisAsyncContext **c = NULL;
    ErlNifBinary cmd;

    if(argc != 2) {
        return enif_make_badarg(env);
    }

    if(!enif_get_resource(env, argv[0], async_context_type, (void**)&c)) {
        return enif_make_badarg(env);
    }
    (void)fprintf(stderr, "resource: p=%p/%p\n", *c, c);

    if(!enif_inspect_binary(env, argv[1], &cmd)) {
        return enif_make_badarg(env);
    }

    ErlNifPid* pid = (ErlNifPid*) enif_alloc(sizeof(ErlNifPid));
    enif_self(env, pid);

    int size = (&cmd)->size;
    char str[size + 1];
    strncpy(str, (char*) (&cmd)->data, size);
    str[size] = '\0';
    printf("cmd %d: [%s]\n", (int) strlen(str), str);
    redisAsyncCommand(*c, getCallback, pid, str);

    return state->atom_ok;
}

static ErlNifFunc nif_funcs[] = {
    {"connect", 2, redis_connect},
    {"async_command", 2, async_command}
};

ERL_NIF_INIT(supredis, nif_funcs, load, unload, NULL, NULL);

