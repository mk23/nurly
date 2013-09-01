NURLY [![Build status](https://travis-ci.org/mk23/nurly.png)](https://travis-ci.org/mk23/nurly)
=====

Nurly is a Nagios Event Broker module that allows service checks to be distributed via libcurl to any compliant http server.  The backend server receiving service check requests is responsible for executing the requested plugin, returning a properly mapped HTTP response code and the output.


Downloading
-----------

- Latest module source: [nurly-module-1.2.5.tar.gz](http://d2g50xfw6d567v.cloudfront.net/mk23/nurly/nurly-module-1.2.5.tar.gz)
- Latest server source: [nurly-server-1.2.5.tar.gz](http://d2g50xfw6d567v.cloudfront.net/mk23/nurly/nurly-server-1.2.5.tar.gz)


Compiling
---------

The build process is through the use of standard autotools command.  If the source is freshly cloned from the repo, first run `autoreconf -i` and then build the module:

    $ ./configure
    $ make
    $ make install

The module will be installed as `/usr/lib/nurly_neb.so`.


Configuration
-------------

### nagios

Nagios needs to have the Execution Broker Module configured and Broker Options set to allow Program State and Service Checks.  Start by editing the main nagios config file (`/etc/nagios3/nagios.cfg`). The parameter to the module is the location of nurly's config file.

    # EVENT BROKER MODULE(S)
    # This directive is used to specify an event broker module that should
    # by loaded by Nagios at startup.  Use multiple directives if you want
    # to load more than one module.  Arguments that should be passed to
    # the module at startup are seperated from the module path by a space.
    broker_module=/usr/lib/nurly_neb.so /etc/nagios3/nurly.cfg

    # EVENT BROKER OPTIONS
    # Controls what (if any) data gets sent to the event broker.
    # Values:  0      = Broker nothing
    #         -1      = Broker everything
    #         <other> = See documentation
    event_broker_options=-1

### nurly

The nurly configuration file is rather self-explanatory.

    ## url for regular service checks
    checks_url = http://localhost:2323/check-command/

    ## url for backend health check
    health_url = http://localhost:2323/server-version

    ## health check interval in seconds
    health_interval = 5

    ## number of nagios worker threads
    worker_threads = 20

    ## seconds before curl gives up and check results in UNKNOWN
    http_timeout = 10

    ## do not distribute checks for these hosts
    skip_host = localhost
    skip_host = special[0-9].*\.com

    ## do not distribute checks for these services
    skip_service = nagios_config
    skip_service = ^SSH$

Operation
---------

The nurly module brokers service checks from Nagios by appending the expanded command-line that Nagios would have otherwise run locally to the url configured and performing the request via libcurl.  This happens in a separate thread, allowing the rest of Nagios to continue operating without waiting for a reply.  The server is expected to perform the requested check and return the output of the check as the body with a specific status code indicating check result.  Nurly recognizes the following HTTP status codes and converts them to proper Nagios result codes:

    HTTP Response    Nagios Result
    200 OK           0 Success/OK
    221 Warning      1 Warning
    222 Critical     2 Critical
    223 Unknown      3 Unknown


Server
------

An example nurly server exists in the `server` subdirectory.  Its feature set includes:

- multiprocess (multi-core) operation, with customizable number of workers
- customizable whitelist of allowed plugin directories
- customizable whitelist of allowed hosts
- embeddable python plugin execution by importing and calling `main()` saving `fork()/exec()` per check

The Nurly server is fully configurable via command-line parameters:

    $ server/nurly.py --help
    usage: nurly.py [-h] [-a SERVER_ADDR] [-p SERVER_PORT] [-n NUM_WORKERS]
                    [-i ALLOWED_IPS [ALLOWED_IPS ...]]
                    [-l PLUGIN_PATH [PLUGIN_PATH ...]] [-t MOD_TIMEOUT]

    nagios service check handler

    optional arguments:
      -h, --help            show this help message and exit
      -a SERVER_ADDR, --server-addr SERVER_ADDR
                            local listening interface (default: )
      -p SERVER_PORT, --server-port SERVER_PORT
                            local listening port (default: 1123)
      -n NUM_WORKERS, --num-workers NUM_WORKERS
                            number of worker processes (default: 2)
      -i ALLOWED_IPS [ALLOWED_IPS ...], --allowed-ips ALLOWED_IPS [ALLOWED_IPS ...]
                            allowed host whitelist, permits all when empty
                            (default: [])
      -l PLUGIN_PATH [PLUGIN_PATH ...], --plugin-path PLUGIN_PATH [PLUGIN_PATH ...]
                            path to plugin directory (default:
                            ['/usr/lib/nagios/plugins'])
      -t MOD_TIMEOUT, --mod-timeout MOD_TIMEOUT
                            module execution timeout (default: 10)


License
-------

TBD
