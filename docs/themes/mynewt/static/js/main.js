jQuery(document).ready(function($) {
  // Global vars
  // TODO: clean-up this ASAP!
  var $window = $(window);
  var wrapper = $('div#wrapper');
  var mobileMenu = $('nav#main-menu-mobile');
  var mobileMenuBars = $('span#nav-bars');
  var searchButton = $('span#nav-search');
  var searchContainer = $('div#search-container');
  var overlayClass = '-overlay';
  var $document = $(document);
  var $header = $('header');
  var $footerHeight = $('footer').height();
  var $body = $('body');
  var currentPos = $document.scrollTop();

  var calculateWindowSize 
  // returns the latest position in range that is smaller or equal than n
  var inRange = function(n, range) {
    var lastCheckpoint = null
    range.forEach(function(val, idx) {
      if (n >= val)
        lastCheckpoint = idx;
    });
    return (lastCheckpoint != null) ? lastCheckpoint : false
  };

  // fades in content as the user scrolls
  var halfScreenElement= function() {
    return document.elementFromPoint(0, $window.height()*.66);
  }

  // This function alternates the different status of the header in body.front
  var headerEffect = function(header, position)  {
    var $elements = [];
    var fadeInElements = [];
    var checkpoints = [];
    var lastCheckpoint = null;
    var currentHalfScreenElement = null;
    $elements.push($('img.logo-region-highlighted'));
    $('div.region-homepage > div.block').each(function(i) {
      $elements.push($(this));
      fadeInElements.push($(this).attr('id'));
    });
    $elements.forEach(function(e, idx) {
      checkpoints.push(e.offset().top);
    });
    lastCheckpoint = inRange(position, checkpoints);
    currentHalfScreenElement = halfScreenElement();
    if ($.inArray(currentHalfScreenElement.id, fadeInElements) != -1) {
      $('div#' + currentHalfScreenElement.id + ' div.container').addClass('-fade-in');
    }

    if (lastCheckpoint !== false) {
      switch(lastCheckpoint) {
        case 0: 
          if ($header.hasClass('-full-header')) {
            $header.addClass('-hidden-full-header');
          } else {
            $header.addClass('-hidden');
          }
          break;
        case 1:
          $header.addClass('-full-header');
          $header.removeClass('-hidden -hidden-full-header');
          break;
        default:
      }
    } else {
      $header.removeClass('-hidden -full-header -hidden-full-header');
    }
  };

  var sidebarEffect = function (sidebar, position, topOffset, bottomOffset) {
    bottomOffset = $document.height() - bottomOffset - sidebar.height();
    console.log(position);
    if (position <= topOffset) {
      sidebar.addClass('sidebar-top');
      sidebar.removeClass('sidebar-fixed sidebar-bottom');
      sidebar.css('top', '');
    } else if (position < bottomOffset) {
      sidebar.removeClass('sidebar-top sidebar-bottom');
      sidebar.addClass('sidebar-fixed');
      sidebar.css('top', '');
    } else {
      lastPosition = sidebar.offset().top - sidebar.parent().offset().top
      if (!sidebar.hasClass('sidebar-bottom')) {
        sidebar.removeClass('sidebar-fixed sidebar-top');
        sidebar.addClass('sidebar-bottom');
        sidebar.css('top', lastPosition);
      }
    }
  };

  var onReadyExec = function() {
    if ($body.hasClass('front')) {
      headerEffect($header, currentPos);
    }

  };
  onReadyExec();
/*
  if ($('body').hasClass('wy-body-for-nav')) {
    $sidebar = $('.wy-side-scroll');
    currentPos = $window.scrollTop();
    $(window).scroll(function() {
      currentPos =  $(this).scrollTop();
      console.log(currentPos);
      sidebarEffect($sidebar, currentPos, 0, 630);
          
    })

  }
  */
  //
  var overlayBehavior = function(action) {
    cssClass = '-overlay';
  }
  var menuMobile = function(action) {
    
  }
  
  var resized =  false;
  $window.resize(function() {
    if ($window.width() > 768 && !resized) {
      resized = true;
    } else {
      resized = false;
    }
  });

  mobileMenuBars.click(function() {
    if (mobileMenu.is(':visible')) {
      mobileMenu.slideUp(200);
      $body.removeClass('-expanded-mobile-menu');
      if (!$body.hasClass('-expanded-search-box')) {
        $body.removeClass('-expanded-header');
        wrapper.removeClass(overlayClass);
      }
    } else {
      mobileMenu.slideDown(200);
      $body.addClass('-expanded-header -expanded-mobile-menu');
      wrapper.addClass(overlayClass);
    }
  });
  
  // shows and hides the search text box
  searchButton.click(function() {
    if (searchContainer.is(':visible')) {
      searchContainer.slideUp(200);
      $('body').removeClass('-expanded-search-box');
      if (!$('body').hasClass('-expanded-mobile-menu')) {
        $('body').removeClass('-expanded-header');
        wrapper.removeClass(overlayClass);
      }
    } else {
      searchContainer.slideDown(200);
      $('body').addClass('-expanded-header -expanded-search-box');
      wrapper.addClass(overlayClass);
    }
  });

  var normalHeaderState = function() {
    body = $('body');
    if (body.hasClass('-expanded-header')) {
      if (searchContainer.is(':visible')) {
        searchContainer.slideUp()
        body.removeClass('-expanded-search-box');
      }
      if (mobileMenu.is(':visible')) {
        mobileMenu.slideUp();
        body.removeClass('-expanded-mobile-menu');
      }
      wrapper.removeClass(overlayClass);
      $('body').removeClass('-expanded-header');
    }
  }
  $('html').click(function() {
    normalHeaderState();
  });

  $('header').click(function(event){
    event.stopPropagation();
  });

  $('span#secondary-menu-button').click(function() {
    if ($(window).width() < 768) { 
      $('div.container-sidebar').toggleClass('-expanded-submenu');
    }
  });

  // when esc key is keyup
  $(document).on('keyup', function(e) {
    if (e.keyCode == 27 && wrapper.hasClass('-overlay')) {
      normalHeaderState();
    }
  });

  // This event listens for a mousedown event on all a.trap-link elements
  // If the user right-clicks the link, the page will scroll to the anchor
  // found on attribute data-hash
  $('a.trap-link').click(function(e) {
    // only do this where body.page-about
    if ($('body').hasClass('page-about')) {
      e.preventDefault();
      var hash = $(this).data('hash');
      $(this).closest('ul.menu').find('a.active').removeClass('active');
      $(this).addClass('active');
      $('html, body').animate({
        scrollTop: $('#' + hash).offset().top - 80 
      }, 500);
    }
  });

  // Listens for position and updates global variable
  $window.scroll(function() {
    currentPos = $(this).scrollTop();
    console.log(currentPos);
    if ($body.hasClass('front')) {
      headerEffect($header, currentPos);
    }
  });

  // TODO: Use global currentPos for scroll position
  if ($('body').hasClass('page-about')) {
    var $menuLinks = [];
    var checkpoints = [];
    var currentPos = 0;
    var lastCheckpoint = null;
    $('div.region-sidebar ul.menu a.trap-link').each(function(i) {
      $menuLinks.push($(this));
      checkpoints[i] = $('#' + $menuLinks[i].data('hash')).offset().top - 100;
    });
    
    $(window).scroll(function() {
      currentPos = $(this).scrollTop();
      lastCheckpoint  = inRange(currentPos, checkpoints);
      if (lastCheckpoint !== false) {
        $menuLinks.forEach(function(e, idx) {
          if (idx != lastCheckpoint) 
            e.removeClass('active');
        });
        $menuLinks[lastCheckpoint].addClass('active');
      }
    });
  }
  $('div.sticky-container').affix({
    offset: {
      top: 0,
      bottom: 635
    }
  });
});

