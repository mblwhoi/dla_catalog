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
      $node = $this->buildNode($nid, 'record', $source->feed_nid);

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

              // If the term did not exist, create it.
              if (! $term){
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
            $item_node = $this->buildNode($v['nid'], 'item', $source->feed_nid);


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
      drupal_set_message(format_plural($batch->created, 'Created @number @type node.', 'Created @number @type nodes.', array('@number' => $batch->created, '@type' => 'record')));
    }
    if ($batch->updated) {
      drupal_set_message(format_plural($batch->updated, 'Updated @number @type node.', 'Updated @number @type nodes.', array('@number' => $batch->updated, '@type' => 'record' )));
    }
    if (! $batch->created && ! $batch->updated){
      drupal_set_message(t('There is no new content.'));
    }
    $batch->setProgress(FEEDS_PROCESSING, FEEDS_BATCH_COMPLETE);
  }

  /**
   * Implementation of FeedsProcessor::clear().
   */
  public function clear(FeedsBatch $batch, FeedsSource $source) {
    drupal_set_message(t('Nothing has been cleared. Delete nodes from normal node management tools.'));
    $batch->setProgress(FEEDS_CLEARING, FEEDS_BATCH_COMPLETE);
  }

  /**
   * Override parent::configForm().
   */
  public function configForm(&$form_state) {
    $author = user_load(array('uid' => $this->config['author']));
    $form['author'] = array(
                            '#type' => 'textfield',
                            '#title' => t('Author'),
                            '#description' => t('Select the author of the nodes to be created - leave empty to assign "anonymous".'),
                            '#autocomplete_path' => 'user/autocomplete',
                            '#default_value' => empty($author->name) ?  'anonymous' : check_plain($author->name),
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
    return $targets;
  }

  /**
   * Creates a new node object in memory and returns it.
   */
  protected function buildNode($nid, $type, $feed_nid) {
    $populate = FALSE;
    $node = new stdClass();
    if (empty($nid)) {
      $node->created = FEEDS_REQUEST_TIME;
      $node->type = $type;
      $node->changed = FEEDS_REQUEST_TIME;
      $node->uid = $this->config['author'];
    }
    else {
      $node = node_load($nid, NULL, TRUE);
    }

    static $included;
    if (!$included) {
      module_load_include('inc', 'node', 'node.pages');
      $included = TRUE;
    }
    node_object_prepare($node);

    // Populate properties that are set by node_object_prepare().
    $node->log = 'Created/updated by FeedsNodeProcessor';

    return $node;
  }

}


