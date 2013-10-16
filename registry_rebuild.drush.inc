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
      return dt('Rebuild the registry or module cache in a Drupal install.');
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
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap.
    'options' => array(
      'no-cache-clear' => 'Rebuild the registry only, do not clear caches, unless --fire-bazooka is also used.',
      'fire-bazooka' => 'Truncate registry and registry_file tables and build them from scratch. Forces all caches clear.',
    ),
    'examples' => array(
      'drush rr --no-cache-clear' => 'Rebuild the registry only, do not clear caches, unless --fire-bazooka is also used.',
      'drush rr --fire-bazooka' => 'Truncate registry and registry_file tables and build them from scratch. Forces all caches clear.',
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
  ini_set('memory_limit', -1);
  if (!drush_bootstrap_to_phase(DRUSH_BOOTSTRAP_DRUPAL_DATABASE)) {
    return drush_set_error('DRUPAL_SITE_NOT_FOUND', dt('You need to specify an alias or run this command within a drupal site.'));
  }
  $include_dir = DRUPAL_ROOT . '/includes';
  $module_dir = DRUPAL_ROOT . '/modules';
  // Use core directory if it exists.
  if (file_exists(DRUPAL_ROOT . '/core/includes/bootstrap.inc')) {
    $include_dir = DRUPAL_ROOT . '/core/includes';
    $module_dir = DRUPAL_ROOT . '/core/modules';
  }

  $includes = array(
    $include_dir . '/bootstrap.inc',
    $include_dir . '/common.inc',
    $include_dir . '/database.inc',
    $include_dir . '/schema.inc',
    $include_dir . '/actions.inc',
    $include_dir . '/entity.inc',
    $module_dir . '/entity/entity.module',
    $module_dir . '/entity/entity.controller.inc',
    $module_dir . '/system/system.module',
    $include_dir . '/database/query.inc',
    $include_dir . '/database/select.inc',
    $include_dir . '/registry.inc',
    $include_dir . '/module.inc',
    $include_dir . '/menu.inc',
    $include_dir . '/file.inc',
    $include_dir . '/theme.inc',
  );

  if (drush_drupal_major_version() == 7) {
    $cache_lock_path_absolute = variable_get('lock_inc');
    if (!empty($cache_lock_path_absolute)) {
      $cache_lock_path_relative = DRUPAL_ROOT . '/'. variable_get('lock_inc');
      // Ensure that the configured lock.inc really exists at that location and
      // is accessible. Otherwise we use the core lock.inc as fallback.
      if (is_readable($cache_lock_path_relative) && is_file($cache_lock_path_relative)) {
        $includes[] = $cache_lock_path_relative;
        drush_log(dt("We will use relative variant of lock.inc: %lock", array('%lock' => $cache_lock_path_relative)));
      }
      elseif (is_readable($cache_lock_path_absolute) && is_file($cache_lock_path_absolute)) {
        $includes[] = $cache_lock_path_absolute;
        drush_log(dt("We will use absolute variant of lock.inc: %lock", array('%lock' => $cache_lock_path_absolute)));
      }
      else {
        drush_log(dt('We will use core implementation of lock.inc as fallback.'));
        $includes[] = DRUPAL_ROOT . '/includes/lock.inc';
      }
    }
    else {
      $includes[] = DRUPAL_ROOT . '/includes/lock.inc';
    }
  }
  elseif (drush_drupal_major_version() > 7) {
    // TODO
    // http://api.drupal.org/api/drupal/namespace/Drupal!Core!Lock/8
  }
  // In Drupal 6 the configured lock.inc is already loaded during
  // DRUSH_BOOTSTRAP_DRUPAL_DATABASE

  foreach ($includes as $include) {
    if (file_exists($include)) {
      require_once($include);
    }
  }

  // This section is not functionally important. It's just using the
  // registry_get_parsed_files() so that it can report the change. Drupal 7 only.
  if (drush_drupal_major_version() == 7) {
    $connection_info = Database::getConnectionInfo();
    $driver = $connection_info['default']['driver'];
    require_once $include_dir . '/database/' . $driver . '/query.inc';
    $parsed_before = registry_get_parsed_files();
  }

  // Separate bootstrap cache exists only in Drupal 7 or newer.
  // They are cleared later again via drupal_flush_all_caches().
  if (drush_drupal_major_version() == 7) {
    cache_clear_all('lookup_cache', 'cache_bootstrap');
    cache_clear_all('variables', 'cache_bootstrap');
    cache_clear_all('module_implements', 'cache_bootstrap');
    drush_log(dt('Bootstrap caches have been cleared in DRUSH_BOOTSTRAP_DRUPAL_DATABASE.'));
  }
  elseif (drush_drupal_major_version() > 7) {
    cache('bootstrap')->deleteAll();
    drush_log(dt('Bootstrap caches have been cleared in DRUSH_BOOTSTRAP_DRUPAL_DATABASE.'));
  }

  if (drush_drupal_major_version() == 7) {
    registry_rebuild();  // Drupal 7 compatible only
  }
  elseif (drush_drupal_major_version() > 7) {
    system_rebuild_module_data();  // Drupal 8 compatible
  }
  else {
    return drush_module_cache_rebuild(); // Drupal 5 and 6 compatible
  }

  drush_log(dt('Bootstrapping to DRUPAL_BOOTSTRAP_FULL.'));
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
  drush_log(dt('Doing registry_rebuild() in DRUPAL_BOOTSTRAP_FULL.'));

  if (!drush_get_option('no-cache-clear')) {
    if (drush_drupal_major_version() >= 7) {
      db_truncate('cache')->execute();
    }
    else {
      cache_clear_all('*', 'cache', TRUE);
    }
    drush_log(dt('The cache table truncated in DRUPAL_BOOTSTRAP_FULL.'));
  }

  if (drush_get_option('fire-bazooka') && drush_drupal_major_version() == 7) {
    drupal_flush_all_caches();
    drush_log(dt('All caches have been cleared with drupal_flush_all_caches.'));
    db_truncate('cache_form')->execute();
    drush_log(dt('The cache_form table truncated in DRUPAL_BOOTSTRAP_FULL.'));
    db_truncate('registry')->execute();
    drush_log(dt('The registry table truncated in DRUPAL_BOOTSTRAP_FULL.'));
    db_truncate('registry_file')->execute();
    drush_log(dt('The registry_file table truncated in DRUPAL_BOOTSTRAP_FULL.'));
  }

  if (drush_drupal_major_version() == 7) {
    registry_rebuild();  // Drupal 7 compatible only
  }
  elseif (drush_drupal_major_version() > 7) {
    system_rebuild_module_data();  // Drupal 8 compatible
  }
  else {
    return drush_module_cache_rebuild(); // Drupal 5 and 6 compatible
  }

  // Extra cleanup available for D7 only.
  if (drush_drupal_major_version() == 7) {
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
      $dt_args = array(
        '@files' => implode(', ', $filenames),
      );
      $singular = 'Manually deleted 1 stale file from the registry.';
      $plural   = 'Manually deleted @count stale files from the registry.';
      drush_log(format_plural(count($filenames), $singular, $plural), 'success');
      $singular = "A file has been declared in a module's .info, but could not be found. This is probably indicative of a bug. The missing file is @files.";
      $plural   = "@count files were declared in a module's .info, but could not be found. This is probably indicative of a bug. The missing files are @files.";
      drush_log(format_plural(count($filenames), $singular, $plural, $dt_args), 'warning');
    }
    $parsed_after = registry_get_parsed_files();
    $message = 'There were @parsed_before files in the registry before and @parsed_after files now.';
    $dt_args = array(
      '@parsed_before' => count($parsed_before),
      '@parsed_after'  => count($parsed_after),
    );
    drush_log(dt($message, $dt_args));
  }

  // Everything done.
  drush_log(dt('All registry rebuilds have been completed.'), 'success');

  if (!drush_get_option('no-cache-clear')) {
    if (drush_drupal_major_version() <= 7) {
      drush_drupal_cache_clear_all();
      cache_clear_all('*', 'cache_form', TRUE);
      drush_log(dt('All caches have been cleared with drush_drupal_cache_clear_all.'));
    }
    else {
      drupal_flush_all_caches();
      db_truncate('cache_form')->execute();
      drush_log(dt('All caches have been cleared with drupal_flush_all_caches.'));
    }
  }
  else {
    drush_log(dt('The caches have not been cleared. It is recommended you clear the Drupal caches as soon as possible.'), 'warning');
  }
}

/**
 * Rebuild for D6 (which really just means rebuilding the menu/system table).
 */
function drush_module_cache_rebuild() {
  drush_log(dt('module_rebuild_cache() in DRUSH_BOOTSTRAP_DRUPAL_DATABASE.'));
  module_list(TRUE, FALSE);
  module_rebuild_cache();
  drush_log(dt('Module cache has been rebuilt.'));
  drush_log(dt('Bootstrapping to DRUPAL_BOOTSTRAP_FULL.'));
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
  if (!drush_get_option('no-cache-clear')) {
    drush_drupal_cache_clear_all();
    cache_clear_all('*', 'cache_form', TRUE);
    drush_log(dt('All caches have been cleared with drush_drupal_cache_clear_all.'));
  }
  else {
    drush_log(dt('The Drupal caches have NOT been cleared after all registry rebuilds.'), 'warning');
    drush_log(dt('It is highly recommended you clear the Drupal caches as soon as possible.'), 'warning');
  }
}
