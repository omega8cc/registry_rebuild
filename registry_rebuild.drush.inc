<?php
/**
 * @file
 * Provide Drush integration for release building and dependency building.
 */

/**
 * Implements hook_drush_help().
 */
function registry_rebuild_drush_help($section) {
  switch ($section) {
    case 'drush:registry-rebuild':
      return dt('Rebuild the registry table in a Drupal install.');
  }
}

/**
 * Implements hook_drush_command().
 */
function registry_rebuild_drush_command() {
  $items = array();

  $items['registry-rebuild'] = array(
    'description' => 'Rebuild the registry table (for classes) and the system table (for module locations) in a Drupal install.',
    'callback' => 'drush_registry_rebuild',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_DATABASE,
    'core' => array('7+'),
    'options' => array(
      'no-cache-clear' => 'Rebuild the registry only, do not clear caches.',
    ),
    'examples' => array(
      'drush rr --no-cache-clear' => 'Rebuild the registry only, do not clear caches.',
    ),
    'aliases' => array('rr'),
  );

  return $items;
}

/**
 * Rebuild the registry.
 *
 * Before calling this we need to be bootstrapped to DRUPAL_BOOTSTRAP_DATABASE.
 */
function drush_registry_rebuild() {
  define('MAINTENANCE_MODE', 'update');
  require_once DRUPAL_ROOT . '/includes/registry.inc';
  require_once DRUPAL_ROOT . '/includes/bootstrap.inc';
  require_once DRUPAL_ROOT . '/includes/common.inc';
  require_once DRUPAL_ROOT . '/includes/entity.inc';
  require_once DRUPAL_ROOT . '/includes/module.inc';
  require_once DRUPAL_ROOT . '/modules/system/system.module';

  require_once DRUPAL_ROOT . '/includes/database/query.inc';
  require_once DRUPAL_ROOT . '/includes/database/select.inc';

  // This section is not functionally important. It's just getting the
  // registry_parsed_files() so that it can report the change.
  $connection_info = Database::getConnectionInfo();
  $driver = $connection_info['default']['driver'];
  require_once DRUPAL_ROOT . '/includes/database/' . $driver . '/query.inc';

  $parsed_before = registry_get_parsed_files();

  cache_clear_all('lookup_cache', 'cache_bootstrap');
  cache_clear_all('variables', 'cache_bootstrap');
  cache_clear_all('module_implements', 'cache_bootstrap');

  drush_log(dt('Doing registry_rebuild() in DRUSH_BOOTSTRAP_DRUPAL_DATABASE.'));
  registry_rebuild();   // At lower level
  drush_log(dt('Registry has been rebuilt.'));

  drush_log(dt('Bootstrapping to DRUPAL_BOOTSTRAP_FULL.'));
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
  drush_log(dt('Doing registry_rebuild() in DRUPAL_BOOTSTRAP_FULL.'));
  registry_rebuild();
  $parsed_after = registry_get_parsed_files();

  // Remove files which don't exist anymore.
  $filenames = array();
  foreach ($parsed_after as $filename => $file) {
    if (!file_exists($filename)) {
      $filenames[] = $filename;
    }
  }

  if (!empty($filenames)) {
    db_delete('registry_file')
      ->condition('filename', $filenames)
      ->execute();
    db_delete('registry')
      ->condition('filename', $filenames)
      ->execute();

    $singular = 'Manually deleted 1 stale file from the registry.';
    $plural   = 'Manually deleted @count stale files from the registry.';
    drush_log(format_plural(count($filenames), $singular, $plural), 'success');
  }

  $parsed_after = registry_get_parsed_files();

  $message = 'There were @parsed_before files in the registry before and @parsed_after files now.';
  $dt_args = array(
    '@parsed_before' => count($parsed_before),
    '@parsed_after'  => count($parsed_after),
  );
  drush_log(dt($message, $dt_args));

  drush_log(dt('The registry has been rebuilt.'), 'success');

  if (!drush_get_option('no-cache-clear')) {
    drush_drupal_cache_clear_all();
  }
  else {
    drush_log(dt('The caches have not been cleared. It is recommended you clear the Drupal caches as soon as possible.'), 'warning');
  }
}
