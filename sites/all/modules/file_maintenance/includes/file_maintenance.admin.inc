<?php

/**
 * The core admin page of the module.
 *
 * @see theme_file_maintenance_content_form()
 */
function file_maintenance_content_form(&$form_state) {
  // Check whether to show the confirmation form or not.
  if (isset($form_state['storage']['confirm']) && $form_state['storage']['confirm']) {
    return confirm_form($form, t('Are you sure you want to %action all files in %directory?', array('%action' => $form_state['storage']['actions']['bulk_action'], '%directory' => $form_state['storage']['directory'])), 'admin/content/file-maintenance');
  }
  else {
    // Add the real form components.
    $form = _file_maintenance_content_form_components();

    // Setup.
    $form['limit']['#value'] = variable_get('file_maintenance_limit', 50);
    $form['show_status']['#value'] = variable_get('file_maintenance_nostatus', 0);

    // Set current dir.
    $form['current_dir']['#value'] = NULL;
    if (isset($_GET['dir']) && !empty($_GET['dir'])) {
      $form['current_dir']['#value'] = urldecode($_GET['dir']);
    }

    // Set filter.
    if (isset($_GET['stay'])) {
      $form['filters']['directory']['#default_value'] = urldecode($_GET['stay']);
    }
    else {
      $form['filters']['directory']['#default_value'] = 1;
    }
    if (isset($_GET['filter']) && !empty($_GET['filter'])) {
      $form['filters']['file_name']['#default_value'] = urldecode($_GET['filter']);
      // Add reset button.
      $form['filters']['reset'] = array(
        '#type' => 'submit',
        '#value' => t('Reset'),
      );
    }

    // Build directory tree.
    $sql_status = 'status = %d';
    if ($form['show_status']['#value']) {
      $sql_status = '';
    }
    $result = file_maintenance_get_dirs($sql_status, FILE_STATUS_PERMANENT);

    // Group all dir forms so they're all together after submit.
    $form['dirtree_forms'] = array('#tree' => TRUE);
    while ($dir = db_fetch_array($result)) {
      $dir = rtrim($dir['dir'], '/');
      // Root is default if nothing was selected on the page unless the filter
      // specifies otherwise.
      if ((empty($form['current_dir']['#value']) && empty($form['filters']['file_name']['#default_value'])) ||
          (empty($form['current_dir']['#value']) && !empty($form['filters']['file_name']['#default_value']) && $form['filters']['directory']['#default_value'])
        ) {
        $form['current_dir']['#value'] = $dir;
      }
      // Add link.
      if ($dir != $form['current_dir']['#value']) {
        $dir_formatted = l(base_path().$dir, 'admin/content/file-maintenance', array('query' => array('dir' => urlencode($dir)), array('attributes' => array('title' => t('Show contents of %dir', array('%dir' => $dir))))));
      }
      else {
        // Highlight current directory.
        $form['current_dir']['#value'] = $dir;
        $dir_formatted = '<strong>'.base_path().$dir.'</strong>';
      }
      $form['dirtree'][$dir]['#value'] = $dir_formatted;
      $form['dirtree_forms'][$dir] = _file_manager_content_form_dir($dir);
    }
    // Build file list.
    $select_all = theme('table_select_header_cell');
    // Center the checkboxes.
    $select_all['class'] .= ' checkbox';
    // Table head.
    $form['files_head']['#value'] = array(
      $select_all,
      array('data' => t('File'), 'field' => 'filename', 'sort' => 'asc'),
      array('data' => t('Size'), 'field' => 'filesize'),
      array('data' => t('Created'), 'field' => 'timestamp'),
      array('data' => t('Owner'), 'field' => 'uid'),
    );

    // Add directory column if filtering across all directories.
    if (!$form['filters']['directory']['#default_value']) {
      $first = array_shift($form['files_head']['#value']);
      $second = array_shift($form['files_head']['#value']);
      array_unshift(
        $form['files_head']['#value'],
        array('data' => t('Directory'), 'field' => 'filepath')
      );
      array_unshift($form['files_head']['#value'], $second);
      array_unshift($form['files_head']['#value'], $first);
    }

    // Add status column.
    if ($form['show_status']['#value']) {
      $last = array_pop($form['files_head']['#value']);
      $form['files_head']['#value'][] = array('data' => t('Status'), 'field' => 'status');
      $form['files_head']['#value'][] = $last;
    }
    $sql_status = 'status = %d';
    if ($form['show_status']['#value']) {
      $sql_status = '';
    }
    
    // Process filter input.
    if (!empty($form['filters']['file_name']['#default_value'])) {
      // Prevent injection.
      $sql_like = db_escape_string($form['filters']['file_name']['#default_value']);
      // Escape the percent char.
      $sql_like = str_replace('%', '\%', $sql_like);
      if (strpos($sql_like, '*') === FALSE) {
        // Add % before and after if no wildcard was used!
        $sql_like = '%'.$sql_like.'%';
      }
      else {
        // Else replace the provided wildcard with an SQL wildcard.
        $sql_like = str_replace('*', '%', $sql_like);
      }
      // Build like statement.
      $sql_like = 'filename LIKE "' .$sql_like. '"';
      $sql_status = empty($sql_status) ? $sql_like : $sql_status . ' AND ' . $sql_like;
    }

    // Get files for current directory.
    $result = file_maintenance_get_files($sql_status, FILE_STATUS_PERMANENT, $form['current_dir']['#value'], $form['files_head']['#value'], $form['limit']['#value']);
    // Group all file checkboxes so they're all together after submit.
    $form['files'] = array('#tree' => TRUE);
    // Store needed file data for theming.
    while ($file = db_fetch_object($result)) {
      // Mark non existant files in red.
      $state = '';
      // This function returns FALSE for files inaccessible due to safe mode
      // restrictions. 
      if (file_exists('.'.base_path().$file->filepath) === FALSE) {
        $state = 'error';
      }
      // Store file data for theming later on.
      $form['files']['fid_'.$file->fid] = array(
        '#type' => 'checkbox',
        '#title' => '',
        '#return_value' => $file->fid,
        '#default_value' => 0,
      );
      $form['filename'][$file->fid]['#value'] = $file->filename;
      $form['filesize'][$file->fid]['#value'] = $file->filesize;
      $form['timestamp'][$file->fid]['#value'] = $file->timestamp;
      $form['uid'][$file->fid]['#value'] = $file->uid;
      $form['exists'][$file->fid]['#value'] = $state;

      // Data for directory column.
      if (!$form['filters']['directory']['#default_value']) {
        $form['filepath'][$file->fid]['#value'] = preg_replace('/(.+)\/.+/i', '$1', $file->filepath);
      }
      // Data for status column.
      if ($form['show_status']) {
        $form['status'][$file->fid]['#value'] = $file->status;
      }
    }
  }

  return $form;
}

/**
 * Helper function to add true form components.
 */
function _file_maintenance_content_form_components() {
  $form = array();

  // Filter form.
  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filter files'),
    '#tree' => TRUE,
  );
  $form['filters']['directory'] = array(
    '#type' => 'checkbox',
    '#title' => t('Stay within current directory'),
    '#description' => t('Uncheck to filter accross all directories.'),
  );
  $form['filters']['file_name'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'textfield',
    '#title' => t('Name and/or extension'),
    '#description' => t('Enter part of the name to filter the files. Use the asterisk (*) as a wildcard.'),
    '#size' => 29,
  );
  $form['filters']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
    '#postfix' => '</div>',
  );

  // Bulk actions form.
  $form['actions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Bulk file actions'),
    '#tree' => TRUE,
  );
  $form['actions']['bulk_action'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'select',
    '#description' => t('Select the action you wish to perform on the selected files.'),
    '#options' => array(
      'move' => t('Move files'),
      'delete' => t('Delete files'),
      'prefix' => t('Prefix files'),
      'remove-prefix' => t('Remove prefix'),
      'postfix' => t('Postfix files'),
      'remove-suffix' => t('Remove suffix'),
    ),
    '#default_value' => !isset($form_state['values']['bulk_action']) ? '' : $form_state['values']['bulk_action'],
  );
  $form['actions']['file_modification'] = array(
    '#type' => 'textfield',
    '#description' => t('Enter new location, filename, prefix or suffix. The suffix is inserted before the file extension!'),
    '#default_value' => '',
    '#size' => 29,
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Apply'),
    '#postfix' => '</div>',
  );

  return $form;
}

/**
 * Helper function to add per directory form.
 */
function _file_manager_content_form_dir($dir) {
  $form = array();

  $form['dir_action'] = array(
    '#prefix' => '<div id="'.str_replace('/', '-', $dir).'" class="container-inline hidden">',
    '#type' => 'textfield',
    '#default_value' => '',
    '#size' => 1,
    '#attributes' => array('class' => 'hidden'),
  );
  $form['dir_modification'] = array(
    '#type' => 'textfield',
    '#default_value' => '',
    '#size' => 29,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Go!'),
    '#suffix' => '</div>',
  );

  return $form;
}

/**
 * Main form validation handler.
 */
function file_maintenance_content_form_validate($form, &$form_state) {
  switch ($form_state['values']['op']) {
    case t('Filter'):
      if (empty($form_state['values']['filters']['file_name'])) {
        form_set_error('filters][file_name', t('You must enter a filename filter!'));
      }
      break;
    case t('Apply'):
      // Clean up results: only include selected files.
      foreach ($form_state['values']['files'] as $key => $fid) {
        if (intval($fid) == 0) {
          unset($form_state['values']['files'][$key]);
        }
      }
      if (!empty($form_state['values']['files'])) {
        // Sort by fid.
        asort($form_state['values']['files']);
      }
      else {
        form_set_error('', t('You must select at least one file!'));
      }
      // Field 'file_modification' can only be empty for the delete action!
      if (empty($form_state['values']['actions']['file_modification'])) {
        $message = _file_maintenance_form_error_message($form_state['values']['actions']['bulk_action']);
        if (!empty($message)) {
          form_set_error('actions][file_modification', $message);
        }
      }
      break;
    case t('Go!'):
      // Check if we have wat we need.
      $fields_ok = FALSE;
      foreach ($form_state['values']['dirtree_forms'] as $dir => $action) {
        if (!empty($action['dir_action']) && !empty($action['dir_modification']) || empty($action['dir_modification']) && $action['dir_action'] == 'delete') {
          $fields_ok = TRUE;
          // Only one dir_action field will be filled in after submit, so when
          // we find it, stop looking.
          break;
        }
        else if (!empty($action['dir_action']) && empty($action['dir_modification'])) {
          $field_error = 'dirtree_forms]['.$dir.'][dir_modification';
          $field_error_id = str_replace('/', '-', $dir);
          $field_error_link = $action['dir_action'] . '-' . $field_error_id;
          $field_action = $action['dir_action'];
        }
      }
      if (!$fields_ok) {
        // Open div when error occured when pressing 'Go!'.
        drupal_add_js(array('fileMaintenance' => array('errorfield' => $field_error_id)), 'setting');
        drupal_add_js(array('fileMaintenance' => array('activelink' => $field_error_link)), 'setting');
        form_set_error($field_error, _file_maintenance_form_error_message($field_action));
      }
      break;
  }
}

function _file_maintenance_form_error_message($action) {
  $message = '';

  switch ($action) {
    case 'move':
      $message = t('You must provide a desitination path!');
      break;
    case 'prefix':
      $message = t('You must provide a prefix!');
      break;
    case 'postfix':
      $message = t('You must provide a suffix!');
      break;
  }

  return $message;
}

/**
 * Main form submit handler.
 */
function file_maintenance_content_form_submit($form, &$form_state) {
  switch ($form_state['values']['op']) {
    case t('Filter'):
      // Keep existing query params.
      $query = $_GET;
      unset($query['q']);
      unset($query['page']);
      // Add our params
      unset($query['stay']);
      if (!$form_state['values']['filters']['directory']) {
        $query['stay'] = urlencode($form_state['values']['filters']['directory']);
        unset($query['dir']);
      }
      $query['filter'] = urlencode($form_state['values']['filters']['file_name']);
      drupal_goto('admin/content/file-maintenance', $query);
      break;
    case t('Apply'):
      // Execute actions on files.
      unset($form_state['values']['actions']['submit']);
      file_maintenance_batch($form_state['values']['files'], $form_state['values']['actions']);
      break;
    case t('Go!'):
      // Execute actions on all files in a directory.
      $form_state['storage']['files'] = array();
      // Filter on file status.
      $sql_status = 'status = %d';
      if (variable_get('file_maintenance_nostatus', 0)) {
        $sql_status = '';
      }
      // Get action and dir.
      foreach ($form_state['values']['dirtree_forms'] as $dir => $action) {
        if (!empty($action['dir_action'])) {
          $form_state['storage']['directory'] = $dir;
          $form_state['storage']['actions']['bulk_action'] = $action['dir_action'];
          $form_state['storage']['actions']['file_modification'] = $action['dir_modification'];
          // Only one dir_action field will be filled in after submit, so when
          // we find it, stop looking.
          break;
        }
      }
      $files = file_maintenance_get_files($sql_status, FILE_STATUS_PERMANENT, $form_state['storage']['directory']);
      if ($files) {
        while ($file = db_fetch_object($files)) {
          $form_state['storage']['files']['fid_'.$file->fid] = $file->fid;
        }
        // Show confirm form when the form is rerendered!
        $form_state['storage']['confirm'] = TRUE;
        $form['rebuild'] = TRUE;
      }
      else {
        drupal_set_message(t('No files found in this directory!'), 'error');
      }
      break;
    case t('Confirm'):
      // User confirmed to a dir based bulk action execution, so start the batch.
      file_maintenance_batch($form_state['storage']['files'], $form_state['storage']['actions']);
    default:
      // Do nothing but reload the page thus resetting everything ;-)
  }
}

/**
 * Build settings form.
 */
function file_maintenance_settings_form() {
  drupal_set_title(t('File maintenance settings'));

  $form = array();

  // General settings.
  $form['file_maintenance_general'] = array(
    '#type' => 'fieldset',
    '#title' => t('General settings'),
    '#collapsible' => FALSE,
  );
  $form['file_maintenance_general']['file_maintenance_limit'] = array(
    '#type' => 'select',
    '#title' => t('Number of files per page'),
    '#description' => t('The higher, the slower the page will load, so be carefull.'),
    '#options' => _file_manager_range(),
    '#default_value' => variable_get('file_maintenance_limit', 50),
  );
  $form['file_maintenance_general']['file_maintenance_delete'] = array(
    '#type' => 'textfield',
    '#title' => t('Replace deleted images with'),
    '#description' => t('When <em>deleting</em> an image from the files table, replace the occurences found in the content with the image given here. <strong>Note that the actual record in the files table will not be removed but updated with this file so as to prevent all references to the file id from breaking.</strong>'),
    '#default_value' => variable_get('file_maintenance_delete', 'sites/default/files/image-removed.jpg'),
  );

  // Advanced settings.
  $form['file_maintenance_advanced'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced settings'),
    '#description' => '<strong>'.t('For advanced users with detailed knowledge of the database only!').'</strong>',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['file_maintenance_advanced']['file_maintenance_nostatus'] = array(
    '#type' => 'checkbox',
    '#title' => t('Do not filter files on status'),
    '#default_value' => variable_get('file_maintenance_nostatus', 0),
    '#description' => t('Tick this if you want to show all files, regardless of their status. A status column will be added in the overview.'),
  );
  $form['file_maintenance_advanced']['file_maintenance_touch'] = array(
    '#type' => 'checkbox',
    '#title' => t('Update timestamp in files table'),
    '#description' => t('Tick this if you want to update the timestamp with each action being performed.'),
    '#default_value' => variable_get('file_maintenance_touch', 0),
  );
  $form['file_maintenance_advanced']['file_maintenance_batch'] = array(
    '#type' => 'textfield',
    '#title' => t('Max number of files to process per batch segment'),
    '#default_value' => variable_get('file_maintenance_batch', 25),
    '#description' => t('All files operations are processed in a multi pass batch. The number given here will reflect the maximum number of files processed per batch pass.'),
    '#size' => 4,
  );
  $form['file_maintenance_advanced']['file_maintenance_copy'] = array(
    '#type' => 'select',
    '#title' => t('What to do when destination file exists'),
    '#options' => array(
     'rename' => t('rename file'),
     'error' => t('throw error'),
    ),
    '#default_value' => variable_get('file_maintenance_copy', 'rename'),
    '#description' => t('Specify what should happen when the destination file exists: <em>rename file</em> will add an incremental number making the new file unique; <em>throw error</em> will do nothing and list an error in the batch results for this file.'),
  );
  $form['file_maintenance_advanced']['file_maintenance_search'] = array(
    '#type' => 'checkbox',
    '#title' => t('Search for and replace file modifications in the tables listed below'),
    '#description' => t('Turning this off might leave you with broken content!'),
    '#default_value' => variable_get('file_maintenance_search', 1),
  );
  $form['file_maintenance_advanced']['file_maintenance_tables'] = array(
    '#type' => 'textarea',
    '#title' => t('Tables and fields you want to search for file occurences'),
    '#default_value' => variable_get('file_maintenance_tables', FILE_MAINTENANCE_DEFAULT_SEARCH),
    '#size' => 60,
    '#description' => t('Enter the name of the table and the field(s) within it that you wish to search. <strong>One table per line omitting table prefixes and curly brackets!</strong><br />Correct formatting: node_revisions|teaser,body'),
  );

  return system_settings_form($form);
}

/**
 * Validate settings form.
 */
function file_maintenance_settings_form_validate($form, &$form_state) {
  $source = realpath($form_state['values']['file_maintenance_delete']);
  if ($form_state['values']['op'] == t('Save configuration') && !file_exists($source)) {
    form_set_error('file_maintenance_delete', t('The file provided does not exist!'));
  }
}

/**
 * Report page callback.
 */
function file_maintenance_report() {
  drupal_set_title(t('File maintenance report'));

  $data = cache_get('file-maintenance-report-data');

  if ($data) {
    return theme('file_maintenance_report', $data);
  }
  else {
    drupal_set_message(t('No <em>File maintenance</em> data found in cache table!'), 'error');
    drupal_goto('admin/content/file-maintenance');
  }
}
