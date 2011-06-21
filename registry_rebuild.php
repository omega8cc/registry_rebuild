<?php

/**
 * Root directory of Drupal installation.
 */
define('DRUPAL_ROOT', realpath(getcwd() . '/../../../..'));
print "DRUPAL_ROOT is " . DRUPAL_ROOT . ".<br/>\n";

global $_SERVER;
$_SERVER['REMOTE_ADDR'] = 'nothing';
require_once DRUPAL_ROOT . '/includes/bootstrap.inc';
require_once DRUPAL_ROOT . '/includes/common.inc';
require_once DRUPAL_ROOT . '/includes/registry.inc';

require_once DRUPAL_ROOT . '/includes/database/query.inc';
require_once DRUPAL_ROOT . '/includes/database/select.inc';

drupal_bootstrap(DRUPAL_BOOTSTRAP_SESSION);

$connection_info = Database::getConnectionInfo();
$driver = $connection_info['default']['driver'];
require_once DRUPAL_ROOT . '/includes/database/' . $driver . '/query.inc';

$parsed_before = registry_get_parsed_files();

cache_clear_all('lookup_cache', 'cache_bootstrap');
cache_clear_all('variables', 'cache_bootstrap');
cache_clear_all('module_implements', 'cache_bootstrap');

registry_rebuild();
$parsed_after = registry_get_parsed_files();

print "There were " . count($parsed_before) . " files in the registry before and " . count($parsed_after) . " files now.<br/>\n";
print "If you don't see any crazy fatal errors, your registry has been rebuilt. You will probably want to flush your caches now.<br/>\n";