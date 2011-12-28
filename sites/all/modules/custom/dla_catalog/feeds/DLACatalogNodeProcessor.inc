<?php

  /**
   * @file
   * Class definition of DLACatalogNodeProcessor.
   */

  /**
   * Creates nodes from feed items.
   */
class DLACatalogNodeProcessor extends FeedsProcessor {

  /**
   * Implementation of FeedsProcessor::process().
   */
  public function process(FeedsImportBatch $batch, FeedsSource $source) {

    // If we don't have term field db info, get it.
    if (! isset($batch->term_field_columns)){
      $batch->term_field_columns = array();
      $term_fields = array('term_id');
      foreach ($term_fields as $tf){
        $tf_info = term_fields_term_fields_api('storage', term_fields_field_load($tf));
        $batch->term_field_columns[$tf] = array_shift(array_keys($tf_info));
      }
    }

    // If we don't have vocabulary ids, get them.
    if (! isset($batch->vocabulary_ids) ){
      $batch->vocabulary_ids = array();
      $batch->fields_vocabularies = array(
                                          'person' => 'Persons',
                                          'cruise' => 'Cruises',
                                          'collection' => 'Collections',
                                          'ship' => 'Ships'
                                          );
      foreach ($batch->fields_vocabularies as $k => $v){
        $result = db_query("SELECT v.vid from {vocabulary} v WHERE v.name = '%s'", $v);
        if ($result){
          $o = db_fetch_object($result);
          $batch->vocabulary_ids[$k] = $o->vid;
        }        
      }
    }

    // Keep track of processed items in this pass, set total number of items.
    $processed = 0;
    if (!$batch->getTotal(FEEDS_PROCESSING)) {
      $batch->setTotal(FEEDS_PROCESSING, count($batch->items));
    }

    // Process each item...
    while ($item = $batch->shiftItem()) {

      // Get nid from item (if present).
      $nid = $item['nid'];

      // Build node in memory.
      $node = $this->buildNode($nid, $source->feed_nid);

      // Map and save node. If errors occur don't stop but report them.
      try {

        // Set title.
        $node->title = $item['title'];

        // Set text fields.
        $text_fields = array(
                             'contributor',
                             'coverage',
                             'creator',
                             'description',
                             'identifier',
                             'language',
                             'publisher',
                             'relation',
                             'rights',
                             'source',
                             'subject'
                             );
        foreach ($text_fields as $f){
          $node->{"field_$f"} = array();
          $values = json_decode($item[$f], TRUE);
          if (is_array($values)){
            foreach ($values as $v){
              $node->{"field_$f"}[] = array('value' => $v);
            }
          }
        }

        // Set date fields.
        $date_fields = array(
                             'date_created'
                             );
        foreach ($date_fields as $f){
          $node->{"field_$f"} = array();
          $values = json_decode($item[$f], TRUE);
          if (is_array($values)){
            foreach ($values as $v){
              $drupal_date = dla_catalog_node_processor_convert_ISO_date_to_drupal_dates($v);
              if ($drupal_date){
                $node->{"field_$f"}[] = $drupal_date;
              }
            }
          }
        }

        // Set normal taxonomy fields.
        $normal_taxonomy_fields = array(
                                        'collection'
                                        );
        foreach ($normal_taxonomy_fields as $f){
          $node->{"field_$f"} = array();
          $values = json_decode($item[$f], TRUE);
          if (is_array($values)){
          
            foreach ($values as $v){

              // Skip if there is no term name.
              if (! $v['name']){
                continue;
              }

              // Get the term's vocabulary id.
              $vid = $batch->vocabulary_ids[$f];

              // Check if the term already exists.
              $result = db_query("SELECT t.tid FROM {term_data} t WHERE t.name = '%s' AND t.vid = '%s'", $v['name'], $vid);

              $term = '';
              if ($result){
                $o = db_fetch_object($result);
                $term = taxonomy_get_term($o->tid);
              }

              // If the term did not exist, create it.
              if (! $term){
                $term = array(
                              'name' => $v['name'],
                              'description' => $v['description'],
                              'weight' => 0,
                              );
              
                $term['vid'] = $vid;

                // Set parent if we have a parent tid.
                $term['parent'] = $parent_tid ? array($parent_tid) : array();
                $term['relations'] = array();
                $term['synonyms'] = '';
              
                // Save term and get it as an object.
                taxonomy_save_term($term);
                $term = taxonomy_get_term($term['tid']);

              }

              // Set the parent tid.
              $parent_tid = $term->tid;

              // Add the term to the taxonomy field's values.
              $node->{"field_$f"}[] = array('value' => $term->tid);
            }
          }
        }
        

        // Set identified taxonomy fields.
        $identified_taxonomy_fields = array(
                                            'cruise',
                                            'person',
                                            'ship'
                                            );

        foreach ($identified_taxonomy_fields as $f){

          $node->{"field_$f"} = array();
          $values = json_decode($item[$f], TRUE);

          if (is_array($values)){

            foreach ($values as $v){

              // Skip if there is no term name or identifier.
              if (! $v['name'] || ! $v['term_id_value']){
                continue;
              }

              // Get the term's vocabulary id.
              $vid = $batch->vocabulary_ids[$f];

              // Try to get the term.
              $result = db_query("SELECT t.tid FROM {term_data} t INNER JOIN {term_fields_term} tf ON t.tid = tf.tid WHERE t.name = '%s' AND tf.%s = '%s' ", $v['name'], $batch->term_field_columns['term_id'], $v['term_id_value']);
              $term = '';
              if ($result){
                $o = db_fetch_object($result);
                $term = taxonomy_get_term($o->tid);
              }

              // If the term did not exist and $create_if_missing is true,  create it.
              if (! $term && $create_if_missing){
                $term = array(
                              'name' => $v['name'],
                              'description' => $v['description'],
                              'weight' => 0,
                              );
              
                $term['vid'] = $vid;
                $term['parent'] = array();
                $term['relations'] = array();
                $term['synonyms'] = '';
    
                $term['fields']['term_id'] = array('value' => $v['term_id_value']);
    
                // Create the term and load it as an object.
                taxonomy_save_term($term);
                $term = taxonomy_get_term($term['tid']);
              }

              if ($term->tid){
                // Add the term to the taxonomy field's values.
                $node->{"field_$f"}[] = array('value' => $term->tid);
              }
            }
          }
        }


        // Set items.
        $node->field_items = array();
        $values = json_decode($item['items'], TRUE);
        if (is_array($values)){
          foreach ($values as $v){

            $item_node = "";

            // Load item node or initialize new node.
            $item_node = $this->buildNode($v['nid'], $source->feed_nid);            
            

            // Set item field values.
            $item_text_fields = array('item_format', 'item_extent', 'item_description');
            foreach ($item_text_fields as $item_f){
              $item_node->{"field_" . $item_f} = array();
              $item_values = $v[$item_f];
              if (is_array($item_values)){
                foreach ($item_values as $item_v){
                  $item_node->{"field_" . $item_f}[] = array('value' => $item_v);
                }
              }
            }

            // Save item node.
            node_save($item_node);

            // Add item node reference to main record node.
            $node->field_items[] = array('nid' => $item_node->nid);
          }
        }

        // Save the node.
        if ($this->config['authorize']) {
          if (empty($node->nid)) {
            $op = 'create';
          }
          else {
            $op = 'update';
          }
          $account = user_load($node->uid);
          if (! node_access($op, $node, $account)) {
            throw new Exception('User ' . $account->uid . ' not authorized to ' . $op . ' content type ' . $node->type);
          }
        }
        node_save($node);

        if (!empty($nid)) {
          $batch->updated++;
        }
        else {
          $batch->created++;
        }
      }
      catch (Exception $e) {
        drupal_set_message($e->getMessage(), 'warning');
        watchdog('feeds', $e->getMessage(), array(), WATCHDOG_WARNING);
      }

      $processed++;
      if ($processed >= variable_get('feeds_node_batch_size', FEEDS_NODE_BATCH_SIZE)) {
        $batch->setProgress(FEEDS_PROCESSING, $batch->created + $batch->updated);
        return;
      }
    }

    // Set messages.
    if ($batch->created) {
      drupal_set_message(format_plural($batch->created, 'Created @number @type node.', 'Created @number @type nodes.', array('@number' => $batch->created, '@type' => node_get_types('name', $this->config['content_type']))));
    }
    elseif ($batch->updated) {
      drupal_set_message(format_plural($batch->updated, 'Updated @number @type node.', 'Updated @number @type nodes.', array('@number' => $batch->updated, '@type' => node_get_types('name', $this->config['content_type']))));
    }
    else {
      drupal_set_message(t('There is no new content.'));
    }
    $batch->setProgress(FEEDS_PROCESSING, FEEDS_BATCH_COMPLETE);
  }

  /**
   * Implementation of FeedsProcessor::clear().
   */
  public function clear(FeedsBatch $batch, FeedsSource $source) {
    if (!$batch->getTotal(FEEDS_CLEARING)) {
      $total = db_result(db_query("SELECT COUNT(nid) FROM {feeds_node_item} WHERE id = '%s' AND feed_nid = %d", $source->id, $source->feed_nid));
      $batch->setTotal(FEEDS_CLEARING, $total);
    }
    $result = db_query_range("SELECT nid FROM {feeds_node_item} WHERE id = '%s' AND feed_nid = %d", $source->id, $source->feed_nid, 0, variable_get('feeds_node_batch_size', FEEDS_NODE_BATCH_SIZE));
    while ($node = db_fetch_object($result)) {
      _feeds_node_delete($node->nid);
      $batch->deleted++;
    }
    if (db_result(db_query_range("SELECT nid FROM {feeds_node_item} WHERE id = '%s' AND feed_nid = %d", $source->id, $source->feed_nid, 0, 1))) {
      $batch->setProgress(FEEDS_CLEARING, $batch->deleted);
      return;
    }

    // Set message.
    drupal_get_messages('status');
    if ($batch->deleted) {
      drupal_set_message(format_plural($batch->deleted, 'Deleted @number node.', 'Deleted @number nodes.', array('@number' => $batch->deleted)));
    }
    else {
      drupal_set_message(t('There is no content to be deleted.'));
    }
    $batch->setProgress(FEEDS_CLEARING, FEEDS_BATCH_COMPLETE);
  }

  /**
   * Implement expire().
   */
  public function expire($time = NULL) {
    if ($time === NULL) {
      $time = $this->expiryTime();
    }
    if ($time == FEEDS_EXPIRE_NEVER) {
      return;
    }
    $result = db_query_range("SELECT n.nid FROM {node} n JOIN {feeds_node_item} fni ON n.nid = fni.nid WHERE fni.id = '%s' AND n.created < %d", $this->id, FEEDS_REQUEST_TIME - $time, 0, variable_get('feeds_node_batch_size', FEEDS_NODE_BATCH_SIZE));
    while ($node = db_fetch_object($result)) {
      _feeds_node_delete($node->nid);
    }
    if (db_result(db_query_range("SELECT n.nid FROM {node} n JOIN {feeds_node_item} fni ON n.nid = fni.nid WHERE fni.id = '%s' AND n.created < %d", $this->id, FEEDS_REQUEST_TIME - $time, 0, 1))) {
      return FEEDS_BATCH_ACTIVE;
    }
    return FEEDS_BATCH_COMPLETE;
  }

  /**
   * Return expiry time.
   */
  public function expiryTime() {
    return $this->config['expire'];
  }

  /**
   * Override parent::configDefaults().
   */
  public function configDefaults() {
    $types = node_get_types('names');
    $type = isset($types['record']) ? 'record' : key($types);
    return array(
                 'content_type' => $type,
                 'input_format' => FILTER_FORMAT_DEFAULT,
                 'update_existing' => FEEDS_SKIP_EXISTING,
                 'expire' => FEEDS_EXPIRE_NEVER,
                 'mappings' => array(),
                 'author' => 0,
                 'authorize' => 0,
                 );
  }

  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {
    $types = node_get_types('names');
    $form = array();
    $form['content_type'] = array(
                                  '#type' => 'select',
                                  '#title' => t('Content type'),
                                  '#description' => t('Select the content type for the nodes to be created. <strong>Note:</strong> Users with "import !feed_id feeds" permissions will be able to <strong>import</strong> nodes of the content type selected here regardless of the node level permissions. Further, users with "clear !feed_id permissions" will be able to <strong>delete</strong> imported nodes regardless of their node level permissions.', array('!feed_id' => $this->id)),
                                  '#options' => $types,
                                  '#default_value' => $this->config['content_type'],
                                  );
    $author = user_load(array('uid' => $this->config['author']));
    $form['author'] = array(
                            '#type' => 'textfield',
                            '#title' => t('Author'),
                            '#description' => t('Select the author of the nodes to be created - leave empty to assign "anonymous".'),
                            '#autocomplete_path' => 'user/autocomplete',
                            '#default_value' => empty($author->name) ?  'anonymous' : check_plain($author->name),
                            );
    $form['update_existing'] = array(
                                     '#type' => 'radios',
                                     '#title' => t('Update existing nodes'),
                                     '#description' => t('Select how existing nodes should be updated. Existing nodes will be determined using mappings that are a "unique target".'),
                                     '#options' => array(
                                                         FEEDS_SKIP_EXISTING => 'Do not update existing nodes',
                                                         FEEDS_REPLACE_EXISTING => 'Replace existing nodes',
                                                         FEEDS_UPDATE_EXISTING => 'Update existing nodes (slower than replacing them)',
                                                         ),
                                     '#default_value' => $this->config['update_existing'],
                                     );
    return $form;
  }

  /**
   * Override parent::configFormValidate().
   */
  public function configFormValidate(&$values) {
    if ($author = user_load(array('name' => $values['author']))) {
      $values['author'] = $author->uid;
    }
    else {
      $values['author'] = 0;
    }
  }


  /**
   * Return available mapping targets.
   */
  public function getMappingTargets() {
    $targets = array();
    drupal_alter('feeds_node_processor_targets', $targets, $this->config['content_type']);
    return $targets;
  }

  /**
   * Creates a new node object in memory and returns it.
   */
  protected function buildNode($nid, $feed_nid) {
    $populate = FALSE;
    $node = new stdClass();
    if (empty($nid)) {
      $node->created = FEEDS_REQUEST_TIME;
      $populate = TRUE;
    }
    else {
      if ($this->config['update_existing'] == FEEDS_UPDATE_EXISTING) {
        $node = node_load($nid, NULL, TRUE);
      }
      else {
        $node->nid = $nid;
        $node->vid = db_result(db_query("SELECT vid FROM {node} WHERE nid = %d", $nid));
        $populate = TRUE;
      }
    }
    if ($populate) {
      $node->type = $this->config['content_type'];
      $node->changed = FEEDS_REQUEST_TIME;
      $node->format = $this->config['input_format'];
      $node->feeds_node_item = new stdClass();
      $node->feeds_node_item->id = $this->id;
      $node->feeds_node_item->imported = FEEDS_REQUEST_TIME;
      $node->feeds_node_item->feed_nid = $feed_nid;
      $node->feeds_node_item->url = '';
      $node->feeds_node_item->guid = '';
    }

    static $included;
    if (!$included) {
      module_load_include('inc', 'node', 'node.pages');
      $included = TRUE;
    }
    node_object_prepare($node);

    // Populate properties that are set by node_object_prepare().
    $node->log = 'Created/updated by FeedsNodeProcessor';
    if ($populate) {
      $node->uid = $this->config['author'];
    }
    return $node;
  }
  }



/*
 * Helper function to format ISO 8601 date string into drupal-compatible date strings
 * will return array('value' => date), or
 * array('value' => start, 'value2' => end) if the iso date is
 * an interval
 */
function dla_catalog_node_processor_convert_ISO_date_to_drupal_dates($iso_date) {

  // array to hold converted dates
  $utc_dates = array();

  // first split the iso date (it could be an interval)
  $iso_dates = explode("/", $iso_date);

  foreach ($iso_dates as $iso_date){

    // skip if not a valid iso date
    if (! dla_catalog_node_processor_is_valid_iso_date($iso_date)){
      next;
    }

    // if date is just a year, add on month and day so that php
    // can recognize it
    if (preg_match('/^\d{4}$/', $iso_date)){
      $iso_date = sprintf("%s-01-01", $iso_date);
    }

    // convert date into PHP DateTime object
    $date_obj = new DateTime($iso_date);

    // Reformat the date into a full ISO date that date_make_date() will accept
    $iso_date_string = date_format_date($date_obj, 'custom', 'Y-m-d\TH:i:s');

    // Create a DateTime object with UTC timezone
    $utc_date = date_make_date($iso_date_string, 'UTC', DATE_DATETIME);

    // Format the UTC version of the DateTime
    $utc_date_string = date_format_date($utc_date, 'custom', 'Y-m-d\TH:i:s');

    // save the date
    $utc_dates[] = $utc_date_string;
  }

  // If there was one date, return an array like ('value' => $date)
  if (count($utc_dates) == 1){
    return array('value' => $utc_dates[0]);
  }

  // Else if there were two dates, return an array like
  // ('value' => $date1, 'value2' => $date2)
  elseif (count($utc_dates == 2)){
    return array(
                 'value' => $utc_dates[0],
                 'value2' => $utc_dates[1]
                 );
  }

  // Otherwise return nothing
  return null;

}

function dla_catalog_node_processor_is_valid_iso_date($iso_date){
  $validation_regex = '/^([\+-]?\d{4}(?!\d{2}\b))((-?)((0[1-9]|1[0-2])(\3([12]\d|0[1-9]|3[01]))?|W([0-4]\d|5[0-2])(-?[1-7])?|(00[1-9]|0[1-9]\d|[12]\d{2}|3([0-5]\d|6[1-6])))([T\s]((([01]\d|2[0-3])((:?)[0-5]\d)?|24\:?00)([\.,]\d+(?!:))?)?(\17[0-5]\d([\.,]\d+)?)?([zZ]|([\+-])([01]\d|2[0-3]):?([0-5]\d)?)?)?)?$/';

  return preg_match($validation_regex, $iso_date);
}

