<?php

/**
 * @file
 * Drush command idi. Instant drupal instantly.
 */

/**
 * Implements hook_drush_command().
 */
function idi_drush_command() {
  $items['idi'] = array(
    'description' => 'Create new virtual host and install Drupal.',
    'callback'    => 'drush_idi',
    'bootstrap'   => DRUSH_BOOTSTRAP_DRUSH,
  );
  return $items;
}

/**
 * Callback for drush command idi.
 */
function drush_idi() {
  $document_root = idi_install_vhost();
  if ($document_root) {
    idi_install_drupal($document_root);
  }
}

/**
 * Add new vhost into your system.
 */
function idi_install_vhost() {

  // Check what script run by sudo.
  $virtual_hosts_dir = "/etc/apache2/sites-available/";
  if (!is_dir($virtual_hosts_dir) || !is_writable($virtual_hosts_dir)) {
    drush_print("\033[0;31mYou must run this script as root - sudo drush idi\033[0m");
    return;
  }

  $default_doc_root = "/var/www";
  $server_alias = "";
  $document_root = "";

  // Get server alias.
  while (!$server_alias) {
    drush_print("Enter name of your new host: ", 0, NULL, FALSE);
    $server_alias = trim(fgets(STDIN));
  }

  // Get document root.
  $project_folder = array_shift(explode('.', $server_alias));
  $default_doc_root = $default_doc_root . '/' . $project_folder;

  // Check what dir is valid.
  $valid_dir = FALSE;
  $denied_dirs = array(
    '/', '.', '/etc', '/bin', '/boot', '/home', '/root',
    '/lib', '/sbin', '/srv', '/var', '/usr', '/dev', '/sys');
  while (!$valid_dir) {
    drush_print("Enter absolute path of your document root [$default_doc_root]: ", 0, NULL, FALSE);
    $line = trim(fgets(STDIN));
    
    if (in_array($line, $denied_dirs)
        || (!empty($line) && $line[0] != '/')) {
      drush_print("\033[0;31mYou should enter absolute path. Do not use /, /etc, /boot and other system directories.\033[0m");
    }
    else {
      $valid_dir = TRUE;
    }
  }

  // Set document root.
  $document_root = (!empty($line)) ? $line : $default_doc_root;

  if (substr($document_root, -1) == '/') {
    $document_root = substr($document_root, 0, strlen($document_root) - 1);
  }

  // Create dir if not exists.
  if (!is_dir($document_root)) {
    mkdir("$document_root/", 0777, TRUE);    
  }

  // Add to hosts.
  $hosts = file_get_contents("/etc/hosts");
  $hosts .= "127.0.0.1\t$server_alias\twww.$server_alias\n";
  file_put_contents("/etc/hosts", $hosts);  

  $host_template = <<<HOST
<VirtualHost *:80>
      ServerAdmin admin@localhost
      ServerName $server_alias
      ServerAlias www.$server_alias

      DocumentRoot $document_root/
      <Directory $document_root/>
              Options Indexes FollowSymLinks MultiViews
              AllowOverride All
              Order allow,deny
              allow from all
      </Directory>

#	ErrorLog \${APACHE_LOG_DIR}/$server_alias-error.log;
      LogLevel warn
#	CustomLog \${APACHE_LOG_DIR}/$server_alias-access.log combined
</VirtualHost>
HOST;

  // Save host file.
  file_put_contents("/etc/apache2/sites-available/$server_alias", $host_template);
  exec("a2ensite $server_alias");

  return $document_root;
}

/**
 * Install drupal and modules.
 */
function idi_install_drupal($droot) {

  if (is_dir($droot)) {    

    // Download Drupal.
    while ($version != 6 && $version != 7) {
      drush_print("Enter Drupal version (6 or 7) [7]: ", 0, NULL, FALSE);
      $version = trim(fgets(STDIN));
      if (empty($version)) {
        $version = 7;
      }
      elseif ($version != 6 && $version != 7) {
        continue;
      }
    }

    drush_print("\033[0;32mPlease wait while Drupal to be downloaded...\033[0m");
    shell_exec("drush dl drupal-$version --destination=$droot");

    $drupal_dirs = scandir("$droot/");
    foreach ($drupal_dirs as $dir) {
      if (preg_match('/drupal-(.)/', $dir)) {
        drush_op('_drush_recursive_copy', "$droot/$dir/", "$droot/");
        drush_delete_dir("$droot/$dir/", TRUE);
        drush_print("\033[0;32mDrupal files moved into $droot\033[0m");
        break;
      }
    }

    $db_params_corectly = FALSE;
    while (!$db_params_corectly) {
      drush_print("Enter data for DB connection. For example - mysql://admin:123456@localhost:3306/testdb");
      $params = trim(fgets(STDIN));
      if (preg_match('/(.){1,}:(.){1,}:(.){1,}@(.){1,}:(.){1,}(.){1,}/', $params)) {
        $db_params_corectly = TRUE;
      }
    }
    chdir($droot);
    // Install Drupal.
    passthru("drush site-install --db-url=$params --account-name=admin --account-pass=admin");
    exec("chmod -R 0777 $droot/");
    drush_print("\033[0;31mPermissions on the directory $droot has been set to 0777. Do not forget to correct it if necessary\033[0m");

    // Restart apache.
    $restart_check = FALSE;
    while (!$restart_check) {
      drush_print("You should restart the apache server. Do you want do this now? (y/n) [y]:", 0, NULL, FALSE);
      $restart = trim(fgets(STDIN));

      if (empty($restart) || strtolower($restart) == 'y') {
        exec("/etc/init.d/apache2 restart");
        drush_print("\033[0;32mInstallation is complete.\033[0m");
        $restart_check = TRUE;
      }
      elseif (strtolower($restart) == 'n') {
        drush_print("\033[0;32mInstallation is complete, but you should restart apache manually.\033[0m");
        $restart_check = TRUE;
      }
    }
  }
  else {
    drush_print("\033[0;31mDrupal installation error. Document root directory is not exists.\033[0m");
  }
}
