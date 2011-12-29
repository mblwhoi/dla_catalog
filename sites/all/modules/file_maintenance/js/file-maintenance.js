// Setup
Drupal.FileMaintenance = Drupal.FileMaintenance || {};
Drupal.FileMaintenance.opendiv = '';
Drupal.FileMaintenance.action = '';
Drupal.FileMaintenance.active = '';

/**
 * Show / hide directory form bulk actions.
 */
Drupal.behaviors.filemaintenance = function() {

  // Expand on error
  if (typeof Drupal.settings.fileMaintenance !== 'undefined') {
    if (Drupal.settings.fileMaintenance.errorfield != '') {
      $('#' + Drupal.settings.fileMaintenance.errorfield).show('fast');
      $('#' + Drupal.settings.fileMaintenance.activelink).addClass('active');
      Drupal.FileMaintenance.active = Drupal.settings.fileMaintenance.errorfield;
    }
  }
  $('.dirlink').each(function() {
      $(this).click(function(e) {
        action = $(this).attr('rel');
        content = $(this).parent().parent().find('.container-inline');

        // Hide 'old' open content.
        if (Drupal.FileMaintenance.opendiv != '' && Drupal.FileMaintenance.opendiv != content.attr('id')) {
          $('#' + Drupal.FileMaintenance.opendiv).hide();
          $('#' + Drupal.FileMaintenance.opendiv).find('input:first').val('');
          Drupal.FileMaintenance.opendiv = '';
          Drupal.FileMaintenance.action = '';
          Drupal.FileMaintenance.active.removeClass('active');
        }
        if (Drupal.FileMaintenance.action == '' || Drupal.FileMaintenance.action == action) {
          // Toggle content.
          if (content.is(':visible')) {
            content.hide('fast');
          }
          else {
            content.show('fast');
          }
        }
        else {
          Drupal.FileMaintenance.active.removeClass('active');
        }

        // Mark link as active.
        if ($(this).hasClass('active')) {
          $(this).removeClass('active');
          content.find('input:first').val('');
        }
        else {
          $(this).addClass('active');
          content.find('input:first').val(action);
        }

        // Store the opened div & clicked action.
        Drupal.FileMaintenance.opendiv = content.attr('id');
        Drupal.FileMaintenance.action = action;
        Drupal.FileMaintenance.active = $(this);

        // Don't follow href.
        e.preventDefault();
      });
  });	
}
