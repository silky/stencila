var $ = window.$ = window.jQuery = require('jquery');
require('jquery.hotkeys');

var utilities = require('../shared/utilities');

window.Stencila = {};

window.stencil = new (function(){

    // Host and port
    var location = window.location;
    this.protocol = location.protocol;
    if(this.protocol==='file:') this.host = 'localfile';
    else this.host = location.hostname;
    this.port = location.port;

    // Address and url
    this.address = null;
    // ... from <meta> tag
    var address = $('head meta[itemprop=address]');
    if(address.length) this.address = address.attr('content');
    // ... or from url
    if(!this.address) {
      // Remove the leading /
      var path = window.location.pathname.substr(1);
      // Remove the last part of path if it is a title slug
      var lastIndex = path.lastIndexOf('/');
      var last = path.substr(lastIndex);
      if(last.substr(last.length-1)=="-") this.address = path.substr(0,lastIndex);
    }
    this.url = this.protocol+'//'+this.host+':'+this.port+'/'+this.address;

    // Boot it up!
    $.ajax({
        type: 'PUT',
        url: this.url+'@boot',
        headers: {
            'Accept' : 'application/json; charset=utf-8',
            'Content-Type': 'application/json; charset=utf-8'
        },
        data: JSON.stringify({}),
    }).done(function(data){
        console.log(data);
    });

    // Update the stencil
    this.refresh = function() {
        // Update the value of inputs first
        $('[data-par] input').each(function(){
            var $input = $(this);
            $input.attr('value', $input.val());
        });
        // Send 
        $.ajax({
            type: 'PUT',
            url: this.url+'@render',
            headers: {
                'Accept' : 'application/json; charset=utf-8',
                'Content-Type': 'application/json; charset=utf-8'
            },
            data: JSON.stringify({
                'format': 'html',
                'content': $('#content').html()
            })
        }).done(function(data){
            $('#content').html(data.content)
        });
    }
    // Delegate so that still on when content is updated
    var self = this;
    $('#content').on('click','.refresh.button',function(){
        self.refresh();
    });
    $(document).bind('keydown', 'ctrl+r', function(){
        self.refresh();
        return false;
    });

    // If any math on the page, configure and load MathJax
    var maths = $('#content').find('script[type^="math/tex"],script[type^="math/asciimath"]');
    if(maths.length){
        window.MathJax = {
          skipStartupTypeset: true,
          showProcessingMessages: false,
          showMathMenu: false,
          "HTML-CSS": {preferredFont: "STIX"}
        };
        utilities.load('/get/web/mathjax/MathJax.js?config=TeX-MML-AM_HTMLorMML', function() {
          // Needs to be don each time stencil is updated
          MathJax.Hub.Queue(
            ["Rerender",MathJax.Hub,"content"],
            function() {
              // Hide math script elements which should now have been rendered into
              // separate display elements by MathJax
              maths.each(function() {
                $(this).css('display','none');
              });
            }
          );
        });
    }

    // Add "decorative" elements
    $('table[data-index] caption, figure[data-index] caption, figure[data-index] figcaption').each(function(){
        var caption = $(this);
        var parent = caption.parent();
        var type = parent.prop("tagName")=='TABLE'?'Table':'Figure';
        var index = parent.attr('data-index');
        caption.prepend('<span>'+type+' '+index+': ');
    });

    // Add automatic styling to certain elements
    $('[data-par]').addClass('ui fluid labeled input');
    $('[data-par]>label').addClass('ui label');
    $('table').addClass('ui celled table');

})();
