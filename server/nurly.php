<?php

$CONFIG = array(
    'allowed_hosts' => array(),
    'allowed_paths' => array(
        '/usr/lib/nagios/plugins',
    ),
    'plugin_timeout' => 10,
);
@include '/etc/nurly.conf';

start_request();
route_request();

function result($code, $text='') {
    static $type = array(
        200 => 'OK',
        221 => 'Warning',
        222 => 'Critical',
        223 => 'Unknown',
        400 => 'Bad Request',
        401 => 'Unauthorized',
        404 => 'Not Found',
    );
    if ($code < 400) {
        header("HTTP/1.0 {$code} {$type[$code]}");
        echo $text;
    } else {
        $head = $type[$code];
        $text = htmlspecialchars(vsprintf($text, array_splice(func_get_args(), 2)));

        header("HTTP/1.0 $code $head");
        echo "<html><head><title>{$_SERVER['REQUEST_URI']}: {$head}</title></head><body><h1>{$head}</h1>{$text}</body></html>\n";
    }

    exit(0);
}

function get_http_code($code) {
    static $http = array(200, 221, 222, 223);

    return isset($http[$code]) ? $http[$code] : $http[sizeof($http) - 1];
}

function start_request() {
    global $CONFIG;

    $hosts = array();
    foreach ($CONFIG['allowed_hosts'] as $host) {
        foreach (gethostbynamel($host) as $addr) {
            $hosts[] = $addr;
        }
    }
    $CONFIG['allowed_hosts'] = $hosts;

    set_time_limit($CONFIG['plugin_timeout']);
}

function route_request() {
    global $CONFIG;

    switch (true) {
        case sizeof($CONFIG['allowed_hosts']) > 0 && !in_array($_SERVER['REMOTE_ADDR'], $CONFIG['allowed_hosts']):

            result(401, "{$_SERVER['REMOTE_ADDR']}: host not allowed");
        case $_SERVER['REQUEST_METHOD'] != 'GET':
            result(400, "{$_SERVER['REQUEST_METHOD']} not allowed.");
        case isset($_SERVER['PATH_INFO']) && $_SERVER['PATH_INFO'] == '/health':
            result(200);
        case isset($_GET['check-command']):
            $path = dirname(array_shift(preg_split('/\s+/', trim($_GET['check-command']))));
            if (!in_array($path, $CONFIG['allowed_paths'])) {
                result(401, "{$path}: illegal plugin path");
            }

            ob_start();
            passthru(escapeshellcmd($_GET['check-command']), $rval);
            result(get_http_code($rval), ob_get_clean());
        default:
            result(404, var_export($_SERVER, true));
    }
}

?>
