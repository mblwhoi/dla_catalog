<?php

  /**
   * @file
   * Class definition of DLACatalogNodeProcessor.
   */

  /**
   * Creates nodes from feed items.
   */
class DLACatalogTermProcessor extends FeedsProcessor {

  /**
   * Implementation of FeedsProcessor::process().
   */
  public function process(FeedsImportBatch $batch, FeedsSource $source) {

    // Keep track of processed items in this pass, set total number of items.
    $processed = 0;
    if (!$batch->getTotal(FEEDS_PROCESSING)) {
      $batch->setTotal(FEEDS_PROCESSING, count($batch->items));
    }

    // Process each item...
    while ($item = $batch->shiftItem()) {

      // Map and save term. If errors occur don't stop but report them.
      try {

        // Array to hold term data.
        $term = array();

        // Get tid from item (if present).
        $tid = $item['tid'];

        // If there was a tid, try to load the term.
        if ($tid) {
          $term = (array) taxonomy_get_term($tid);
          if (! $term){
            throw new Exception("No term with tid '$tid' exists, item was skipped.");
          }
        }

        // Set blank defaults.
        $term['relations'] = array();
        $term['synonyms'] = '';

        // Set simple term properties from item.
        $simple_term_properties = array('name', 'description', 'weight', 'vid');
        foreach ($simple_term_properties as $p){
          $term[$p] = $item[$p];
        }

        // Set parent if present.
        $term['parent'] = $item['parent'] ? array($item['parent']) : array();

        // Set term field values from item.
        $term_fields = array('term_id');
        foreach ($term_fields as $f){
          $term['fields'][$f] = array('value' => $item[$f]);
        }

        // Save the term.
        taxonomy_save_term($term);

        if (! empty($item['tid'])) {
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
      drupal_set_message(format_plural($batch->created, 'Created @number term.', 'Created @number terms.', array('@number' => $batch->created)));
    }
    if ($batch->updated) {
      drupal_set_message(format_plural($batch->created, 'Updated @number term.', 'Updated @number terms.', array('@number' => $batch->updated)));
    }
    if (! $batch->created && ! $batch->updated){
      drupal_set_message(t('There are no new terms.'));
    }
    $batch->setProgress(FEEDS_PROCESSING, FEEDS_BATCH_COMPLETE);
  }


  /**
   * Implementation of FeedsProcessor::clear().
   */
  public function clear(FeedsBatch $batch, FeedsSource $source) {
    drupal_set_message(t('Nothing has been cleared. Delete terms from normal term management tools.'));
    $batch->setProgress(FEEDS_CLEARING, FEEDS_BATCH_COMPLETE);
  }

  /**
   * Return available mapping targets.
   */
  public function getMappingTargets() {
    $targets = array();
    return $targets;
  }


}

