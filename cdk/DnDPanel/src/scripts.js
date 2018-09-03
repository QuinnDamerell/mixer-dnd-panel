
const dndFooterHeight = 100;
const dndFooterBorderWidth = 1;

// Setup the window.
window.addEventListener('load', function initMixer() {

  // Setup the handler to handle resizes.
  mixer.display.position().subscribe(handleVideoResized);

  // Move the video by a static offset amount
  mixer.display.moveVideo({
    top: 0,
    bottom: dndFooterHeight,
    left: 0,
    right: 0,
  });

  // Move the overlay by the same amount.
  document.getElementById('overlay').bottom = dndFooterHeight;
  document.getElementById('footer').bottom = dndFooterHeight;

  mixer.socket.on('event', data => {
    const playerValue = data.player_values[0];
    let newNegWidth = playerValue < 0 ? (playerValue * -1) / 2 : 0;
    let newNegLeft = playerValue < 0 ? (100 - ((playerValue * -1) / 2)) + 65 : 165;
    let newPosWidth = playerValue > 0 ? (playerValue) / 2 : 0;
    $(".player-influence-neg-0").animate({
      width: newNegWidth,
      left: newNegLeft
    });
    $(".player-influence-pos-0").animate({
      width: newPosWidth
    });
    console.log('Event Update: ', data);
  })

  // Whenever someone clicks on "Hello World", we'll send an event
  // to the game client on the control ID "hello-world"
  // document.getElementById('test-button').onclick = function(event) {
  //   mixer.socket.call('giveInput', {
  //     controlID: 'tunnel-button',
  //     event: 'click',
  //     button: event.button,
  //   });
  // };

  $(".player-minus-0").click(function() {
    mixer.socket.call('giveInput', {
      controlID: 'tunnel-button',
      action: "player-influence",
      event: 'click',
      player: 0,
      isAdd : false
    });
  });
  $(".player-plus-0").click(function() {
    mixer.socket.call('giveInput', {
      controlID: 'tunnel-button',
      action: "player-influence",
      event: 'click',
      player: 0,
      isAdd : true
    });
  });

  // Player stuff.
  $(".player-carl").hover(
    function(){
      $(this).find(".popup").fadeIn("slow");
    },
    function(){
      $(this).find(".popup").fadeOut("slow");
    }
  );
  $(".player-other").hover(
    function(){
      $(this).find(".popup").fadeIn("slow");
    },
    function(){
      $(this).find(".popup").fadeOut("slow");
    }
  );
  $(".player-other2").hover(
    function(){
      $(this).find(".popup").fadeIn("slow");
    },
    function(){
      $(this).find(".popup").fadeOut("slow");
    }
  );

  mixer.isLoaded();
});


function handleVideoResized (position) {
  const overlay = document.getElementById('overlay');
  const footer = document.getElementById('footer');
  const player = position.connectedPlayer;
  overlay.style.top = `${player.top}px`;
  overlay.style.left = `${player.left}px`;
  overlay.style.height = `${player.height}px`;
  overlay.style.width = `${player.width}px`;
  footer.style.left = `${player.left}px`;
  footer.style.width = `${player.width - 10}px`;
  footer.style.height = `80px`;
  footer.style.top = `${player.top + player.height}px`;
}
