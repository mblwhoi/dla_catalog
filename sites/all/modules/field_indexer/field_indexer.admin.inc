<?php
// $Id: field_indexer.admin.inc,v 1.1 2009/01/05 01:30:46 davidlesieur Exp $

/**
 * @file
 * Administrative page callbacks for the Field Indexer module.
 */

/**
 * Menu callback for configuring the module.
 */
function field_indexer_admin($form_state) {
  // Find all the fields available for indexing.
  $fields = field_indexer_find_fields();

  if (empty($fields)) {
    $form['message'] = array(
      '#type' => 'markup',
      '#value' => t('No fields available for indexing.'),
    );
    return $form;
  }

  // Find which fields were already configured.
  $known_fields = field_indexer_load_fields();

  // Assign fiid and status to fields.
  foreach ($fields as $key => $field) {
    if (isset($known_fields[$key])) {
      // Preserve the known fiid and status.
      $fields[$key]['fiid'] = $known_fields[$key]['fiid'];
      $fields[$key]['status'] = $known_fields[$key]['status'];
      // Remove from array so at the end we have a list of fields that no longer
      // exist on the system.
      unset($known_fields[$key]);
    }
  }

  // Build the list of options.
  $options = array();
  $defaults = array();
  foreach ($fields as $key => $field) {
    // TODO: Format field label and help in a nice table instead.
    $help = check_plain($field['namespace']) . (empty($field['help']) ? '' : ' / '. check_plain($field['help']));
    $options[$key] = check_plain($field['label']) .' ('. $help .')';
    if ($field['status']) {
      $defaults[] = $key;
    }
  }

  // Build the settings form.
  $form['selector'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Fields to index'),
    '#description' => t('Check the fields that need to be indexed.'),
    '#options' => $options,
    '#default_value' => $defaults,
  );
  $form['fields'] = array(
    '#type' => 'value',
    '#value' => $fields,
  );
  $form['lost_fields'] = array(
    '#type' => 'value',
    '#value' => $known_fields,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;
}

/**
 * Submit handler for the settings.
 */
function field_indexer_admin_submit($form, &$form_state) {
  $enabled = FALSE;

  // Update or insert field selection.
  foreach ($form_state['values']['fields'] as $key => $field) {
    $field['status'] = $form_state['values']['selector'][$key] ? 1 : 0;
    if (isset($field['fiid'])) {
      // Determine whether the field needs an update.
      if (db_result(db_query("SELECT COUNT(*) FROM {field_indexer_map} WHERE fiid = %d AND (status <> %d OR label <> '%s')", $field['fiid'], $field['status'], $field['label']))) {
        // Update the existing field.
        drupal_write_record('field_indexer_map', $field, 'fiid');
        
        if ($field['status']) {
          $enabled = TRUE;
        }
        else {
          // Field is disabled, delete its index entries.
          field_indexer_wipe($field['fiid']);
        }
      }
    }
    else {
      // Insert newly discovered field.
      drupal_write_record('field_indexer_map', $field);
      
      if ($field['status']) {
        $enabled = TRUE;
      }
    }
  }

  // Delete obsolete fields.
  foreach ($form_state['values']['lost_fields'] as $field) {
    // Delete obsolete field data.
    db_query('DELETE FROM {field_indexer_map} WHERE fiid = %d', $field['fiid']);

    // Delete index entries for the field.
    field_indexer_wipe($field['fiid']);
  }

  if ($enabled) {
    field_indexer_reindex(); // Re-index all nodes.
    drupal_set_message(t('All nodes have been marked for re-indexing to ensure the indexing of the newly enabled fields. Indexing will start on the next <a href="@cron">cron</a> run.', array('@cron' => url('admin/reports/status/run-cron'))));
  }

  drupal_set_message(t('The configuration options have been saved.'));
}


