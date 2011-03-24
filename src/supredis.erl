-module(supredis).
-export([connect/2,
         sync_command/2,
         sync_command/3,
         async_command/2]).
-on_load(init/0).

-define(APPNAME, supredis).
-define(LIBNAME, supredis).

connect(_, _) ->
    not_loaded(?LINE).

sync_command(Conn, Cmd) when is_binary(Cmd) ->
    sync_command(Conn, Cmd, 8000).

sync_command(Conn, Cmd, Timeout) when is_binary(Cmd), is_integer(Timeout) ->
    async_command(Conn, Cmd),
    Self = self(),
    receive
        {supredis, Self, Reply} ->
            Reply
    after Timeout ->
        %% NOTE: msg will still be pushed into process message queue
        {error, timeout}
    end.
    
async_command(_, _) ->
    not_loaded(?LINE).

init() ->
    SoName = case code:priv_dir(?APPNAME) of
        {error, bad_name} ->
            case filelib:is_dir(filename:join(["..", priv])) of
                true ->
                    filename:join(["..", priv, ?LIBNAME]);
                _ ->
                    filename:join([priv, ?LIBNAME])
            end;
        Dir ->
            filename:join(Dir, ?LIBNAME)
    end,
    erlang:load_nif(SoName, 0).

not_loaded(Line) ->
    exit({not_loaded, [{module, ?MODULE}, {line, Line}]}).

