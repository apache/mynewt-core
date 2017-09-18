jQuery(document).ready(function($) {
	$("span#secondary-menu-button").click(function() {
		if ($(window).width() < 768) {
			$("div.container-sidebar").toggleClass("-expanded-submenu");
		}
	});

	getNavbarOffset = function() {
		return $("#banner").height();
	};

	$("#navbar").affix({
		offset: { top: getNavbarOffset }
	});

	$(window).resize(function() {
		$("#navbar").affix("checkPosition");
	});
});

function manageDocSidebar() {
	if (window.matchMedia("(max-width: 992px)").matches) {
		// menu above content
		$("#docSidebar").css("height", "initial");
		$("#docSidebar").css("overflow-y", "initial");
	} else {
		// side by side - menu + content
		var sidebarHeight = $("#docSidebar").height();
		console.log("sidebar at " + sidebarHeight);

		var visibleHeight = $(window).height() - $("#navbar").height();
		console.log("visible at " + visibleHeight);

		// Fix the sidebar height to the visible window
		$("#docSidebar").css("height", visibleHeight);

		// If the sidebar content is larger than visible height give it scrollability.
		if (sidebarHeight <= visibleHeight) {
			$("#docSidebar").css("overflow-y", "hidden");
		} else {
			$("#docSidebar").css("overflow-y", "scroll");
		}
	}
}

$(document).ready(function() {
	$("#docSidebar").css("overflow-x", "hidden");
	manageDocSidebar();
	$(window).on("resize", manageDocSidebar);
});
