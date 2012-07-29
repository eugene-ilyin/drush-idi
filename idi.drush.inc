<?php

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
  idi_install_drupal($document_root);
}

/**
 * Add new vhost into your system.
 */
function idi_install_vhost() {

  // Check what script run by sudo.
  $virtual_hosts_dir = "/etc/apache2/sites-available/";
  if (!is_dir($virtual_hosts_dir) || !is_writable($virtual_hosts_dir)) {
    echo "\033[0;31mYou must run this script as root!\n\033[0m";
    exit;
  }

  $default_doc_root = "/var/www";
  $server_alias = "";
  $document_root = "";

  // Get server alias.
  while (!$server_alias) {
    echo "Enter name of your new host: ";
    $server_alias = trim(fgets(STDIN));
  }

  // Get document root.
  $project_folder = array_shift(explode('.', $server_alias));
  $default_doc_root = $default_doc_root . '/' . $project_folder;
  echo "Enter absolute path of your document root [$default_doc_root]: ";
  $line = trim(fgets(STDIN));

  $document_root = (!empty($line)) ? $line : $default_doc_root;

  if (substr($document_root, -1) == '/') {
    $document_root = substr($document_root, 0, strlen($document_root) - 1);
  }

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
      echo "Enter Drupal version (6 or 7) [7]: ";
      $version = trim(fgets(STDIN));
      if (empty($version)) {
        $version = 7;
      }
      elseif ($version != 6 && $version != 7) {
        continue;
      }
    }

    echo "\033[0;32mPlease wait while Drupal to be downloaded...\n\033[0m";
    shell_exec("drush dl drupal-$version --destination=$droot");
    exec("mv -f $droot/drupal-$version*/* $droot/");
    echo "\033[0;32mDrupal files moved into $droot\n\n\033[0m";
    exec("rm -R $droot/drupal-$version*");

    $db_params_corectly = FALSE;
    while (!$db_params_corectly) {
      echo "echo Enter data for DB connection. For example - mysql://admin:123456@localhost:3306/testdb\n";
      $params = trim(fgets(STDIN));
      if (preg_match('/(.){1,}:(.){1,}:(.){1,}@(.){1,}:(.){1,}(.){1,}/', $params)) {
        $db_params_corectly = TRUE;
      }
    }
    chdir($droot);
    // Install Drupal.
    passthru("drush site-install --db-url=$params --account-name=admin --account-pass=admin");
    exec("chmod -R 0777 $droot/");
    echo "\033[0;31mPermissions on the directory $droot has been set to 0777. Do not forget to correct it if necessary\n\033[0m";

    // Create .htaccess file if he is not exists.
    if (!file_exists("$droot/.htaccess")) {
      $htaccess = <<<HTACCESS
#
# Apache/PHP/Drupal settings:
#

# Protect files and directories from prying eyes.
<FilesMatch "\.(engine|inc|info|install|make|module|profile|test|po|sh|.*sql|theme|tpl(\.php)?|xtmpl|svn-base)$|^(code-style\.pl|Entries.*|Repository|Root|Tag|Template|all-wcprops|entries|format)$">
  Order allow,deny
</FilesMatch>

# Don't show directory listings for URLs which map to a directory.
Options -Indexes

# Follow symbolic links in this directory.
Options +FollowSymLinks

# Make Drupal handle any 404 errors.
ErrorDocument 404 /index.php

# Force simple error message for requests for non-existent favicon.ico.
<Files favicon.ico>
  # There is no end quote below, for compatibility with Apache 1.3.
  ErrorDocument 404 "The requested file favicon.ico was not found.
</Files>

# Set the default handler.
DirectoryIndex index.php

# Override PHP settings. More in sites/default/settings.php
# but the following cannot be changed at runtime.

# PHP 4, Apache 1.
<IfModule mod_php4.c>
  php_value magic_quotes_gpc                0
  php_value register_globals                0
  php_value session.auto_start              0
  php_value mbstring.http_input             pass
  php_value mbstring.http_output            pass
  php_value mbstring.encoding_translation   0
</IfModule>

# PHP 4, Apache 2.
<IfModule sapi_apache2.c>
  php_value magic_quotes_gpc                0
  php_value register_globals                0
  php_value session.auto_start              0
  php_value mbstring.http_input             pass
  php_value mbstring.http_output            pass
  php_value mbstring.encoding_translation   0
</IfModule>

# PHP 5, Apache 1 and 2.
<IfModule mod_php5.c>
  php_value magic_quotes_gpc                0
  php_value register_globals                0
  php_value session.auto_start              0
  php_value mbstring.http_input             pass
  php_value mbstring.http_output            pass
  php_value mbstring.encoding_translation   0
</IfModule>

# Requires mod_expires to be enabled.
<IfModule mod_expires.c>
  # Enable expirations.
  ExpiresActive On

  # Cache all files for 2 weeks after access (A).
  ExpiresDefault A1209600

  <FilesMatch \.php$>
    ExpiresActive Off
  </FilesMatch>
</IfModule>

# Various rewrite rules.
<IfModule mod_rewrite.c>
  RewriteEngine on
  # Rewrite URLs of the form 'x' to the form 'index.php?q=x'.
  RewriteCond %{REQUEST_FILENAME} !-f
  RewriteCond %{REQUEST_FILENAME} !-d
  RewriteCond %{REQUEST_URI} !=/favicon.ico
  RewriteRule ^(.*)$ index.php?q=$1 [L,QSA]
</IfModule>

# $Id$
HTACCESS;

      file_put_contents("$droot/.htaccess", $htaccess);
      echo"\033[0;32mhtacess file generated by \"drush idi\"\n\033[0m";
    }

    // Restart apache.
    $restart_check = FALSE;
    while (!$restart_check) {
      echo "You must restart the server apache. Do you want do this now? (y/n) [y]";
      $restart = trim(fgets(STDIN));

      if (empty($restart) || strtolower($restart) == 'y') {
        exec("/etc/init.d/apache2 restart");
        echo "\033[0;32mInstallation is complete.\n\033[0m";
        $restart_check = TRUE;
      }
      elseif (strtolower($restart) == 'n') {
        echo "\033[0;32mInstallation is complete, but you should restart apache manually.\n\033[0m";
        $restart_check = TRUE;
      }
    }
  }
  else {
    echo "\033[0;31mDrupal installation error. Document root directory is not exists.\n\033[0m";
  }
}
