all:
	git submodule update --init
	(cd deps/hiredis;$(MAKE))
	./rebar compile

clean:
	(cd deps/hiredis;$(MAKE) clean)
	./rebar clean
