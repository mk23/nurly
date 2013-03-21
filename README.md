NURLY
=====

Nurly is a Nagios Event Broker module that allows service checks to be distributed via libcurl to any compliant http server.  The backend server receiving service check requests is responsible for executing the requested plugin, returning a properly mapped HTTP response code and the output.


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
    health_url = http://localhost:2323/nurly-version

    ## health check interval in seconds
    health_interval = 5

    ## number of nagios worker threads
    worker_threads = 10


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
