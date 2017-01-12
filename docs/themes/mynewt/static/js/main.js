jQuery(document).ready(function($) {
  $('span#secondary-menu-button').click(function() {
    if ($(window).width() < 768) {
      $('div.container-sidebar').toggleClass('-expanded-submenu');
    }
  });

  getNavbarOffset = function() {
    return $('#banner').height()
  }

  $('#navbar').affix({
    offset: { top: getNavbarOffset }
  })

  $(window).resize(function() {
    $('#navbar').affix('checkPosition')
  })
});
