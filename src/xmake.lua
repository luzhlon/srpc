target 'srpc'
    set_kind 'shared'

    set_targetdir('$(buildir)/$(mode)/$(arch)')

    add_headers '*.h'
    -- add_headers '../deps/xval/src/*.h'
    add_files '*.cpp'
    add_files '../deps/xval/src/*.cpp'

    add_includedirs('../deps/tstream/src',
                    '../deps/xval/src')

    add_linkdirs('$(buildir)')
    -- add_links('websocket')

    add_defines 'DLL_EXPORT'

    if is_mode 'debug' then
        add_cxxflags '/MDd'
    end
