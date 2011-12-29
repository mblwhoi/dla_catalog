<?php

/**
 * Theme maintenance form.
 *
 * @ingroup themeable
 */
function theme_file_maintenance_content_form($form) {
  $dirtable = $filetable = '';

  // Theme directory tree if there is one.
  if (isset($form['dirtree'])) {
    drupal_add_css(drupal_get_path('module', 'file_maintenance').'/css/file-maintenance.css', 'module');
    drupal_add_js(drupal_get_path('module', 'file_maintenance').'/js/file-maintenance.js', 'module');
    $head = array(
      t('Directories'),
      array('data' => t('Actions'), 'colspan' => 4),
    );

    $dirtree = array();
    foreach (element_children($form['dirtree']) as $dir) {
      $dirtree[] = array(
        array('data' => $form['dirtree'][$dir]['#value'] . drupal_render($form['dirtree_forms'][$dir])),
        array('data' => _file_manager_format_directory(t('Move contents'), 'move', $dir)),
        array('data' => _file_manager_format_directory(t('Delete contents'), 'delete', $dir)),
        array('data' => _file_manager_format_directory(t('Prefix all files'), 'prefix', $dir)),
        array('data' => _file_manager_format_directory(t('Postfix all files'), 'postfix', $dir)),
      );
    }
    $dirtable = theme_table($head, $dirtree);
    unset($form['dirtree']);
    unset($form['dirtree_forms']);
  }

  // Theme file list if there are any files.
  if (isset($form['filename'])) {
    $file_states = array(
      t('Temporary'),
      t('Permanent'),
    );

    foreach (element_children($form['filename']) as $fid) {
      // Build row.
      $row = array(
        array('data' => drupal_render($form['files']['fid_'.$fid]), 'class' => 'checkbox'),
        array('data' => $form['filename'][$fid]['#value'], 'class' => $form['exists'][$fid]['#value']),
        array('data' => format_size($form['filesize'][$fid]['#value']), 'class' => $form['exists'][$fid]['#value']),
        array('data' => format_date($form['timestamp'][$fid]['#value'], 'medium'), 'class' => $form['exists'][$fid]['#value']),
        array('data' => _file_manager_format_user($form['uid'][$fid]['#value']), 'class' => $form['exists'][$fid]['#value']),
      );

      // Add directory column if filtering across all directories.
      if (!$form['filters']['directory']['#default_value']) {
        $first = array_shift($row);
        $second = array_shift($row);
        array_unshift(
          $row,
          array('data' => $form['filepath'][$fid]['#value'], 'class' => $form['exists'][$fid]['#value'])
        );
        array_unshift($row, $second);
        array_unshift($row, $first);
      }

      // Insert status field.
      if ($form['show_status']['#value']) {
        $last = array_pop($row);
        $row[] = array('data' => $file_states[$form['status'][$fid]['#value']], 'class' => $form['exists'][$fid]['#value']);
        $row[] = $last;
      }

      // Compose final row.
      $rows[] = array(
        'data' => $row,
        'class' => $form['exists'][$fid]['#value'],
      );
    }

    // Remove theming data to clean up form.
    $limit = $form['limit']['#value'];
    $head = $form['files_head']['#value'];
    unset($form['limit']);
    unset($form['show_status']);
    unset($form['current_dir']);
    unset($form['files_head']);
    unset($form['filename']);
    unset($form['filepath']);
    unset($form['filesize']);
    unset($form['timestamp']);
    unset($form['uid']);
    unset($form['exists']);
    unset($form['status']);

    $filetable .= theme_table($head, $rows);
    $filetable .= theme('pager', NULL, $limit);
  }
  else {
    // No files found, unset what we don't need.
    unset($form['limit']);
    unset($form['show_status']);
    unset($form['current_dir']);
    unset($form['files_head']);
    unset($form['actions']);
    $filetable .= t('No records found in your %table table!', array('%table' => db_prefix_tables('{files}')));
  }
  // Add buttons. You need to render the 'full' form (or what's left of it) to
  // make the _submit($form, &$form_state) handler trigger.
  return $dirtable . drupal_render($form).'<br />'. $filetable;
}

/**
 * Helper function to show user link.
 */
function _file_manager_format_user($uid) {
  $sql = 'SELECT name FROM {users} WHERE uid = %d';
  $user = db_result(db_query($sql, $uid));
  return l($user, 'user/'.$uid.'/edit', array('attributes' => array('title' => t('Edit user'))));
}

/**
 * Helper function to show directory link.
 * Using l() here is not needed and even impossible if you want the link to be a
 * hash (#).
 */
function _file_manager_format_directory($link_title, $action, $dir) {
  return '<a id="'.$action.'-'.str_replace('/', '-', $dir).'" class="dirlink" rel="'.$action.'" title="'.$link_title.'" href="#">'.$link_title.'</a>';
}

/**
 * Theme the report page.
 *
 * @param $cache
 *   Drupal cache data object.
 *
 * @return string
 */
function theme_file_maintenance_report($cache) {
  $output = '';
  $rows = array();
  $search = variable_get('file_maintenance_search', 1);

  $backlink = l(t('Back to <em>File maintenance</em>'), 'admin/content/file-maintenance', array('html' => TRUE, 'attributes' => array('title' => t('Back to File maintenance'))));

  $output .= '<p>';
  $output .= t('Report generated on %time:', array('%time' => format_date($cache->created, 'long'))).' <strong>'.$cache->data['message'].'</strong>';
  unset($cache->data['message']);
  // Add back link at the top of the page if it's getting big.
  if (count($cache->data) > 40) {
    $output .= '<br />'.$backlink;
  }
  $output .= '</p>';

  $header = array(t('Action performed'));
  if ($search) {
    $header[] = t('Database search and replace results');
  }

  foreach ($cache->data as $fid => $result) {
    $row = array(array('data' => $result['message'], 'class' => $result['status']));
    if ($search) {
      $search_message = '';
      // $result['results'] is not set when there was an error!
      if (isset($result['results'])) {
        foreach ($result['results'] as $table => $affected) {
          $search_message .= t('Replaced %count occurences in %table.', array('%count' => $affected, '%table' => db_prefix_tables('{'.$table.'}'))).'<br />';
        }
      }
      $row[] = array('data' => rtrim($search_message, '<br />'), 'class' => $result['status']);
    }
    // Compose final row.
    $rows[] = array(
      'data' => $row,
      'class' => $result['status'],
    );
  }

  $output .= '<br />' . theme_table($header, $rows);
  $output .= $backlink;

  return $output;
}
