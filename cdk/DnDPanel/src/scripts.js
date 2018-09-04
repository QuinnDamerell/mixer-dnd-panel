const dndFooterHeight = 110;
const dndFooterBorderWidth = 1;

let adjustButtonsDisabled = false;

// Setup the window.
window.addEventListener('load', function initMixer() {

  // Setup the handler to handle resizes.
  mixer.display.position().subscribe(handleVideoResized);
  setTimeout(setupInitialPositions, 1000);

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

  // Remove all of the bars to start
  $('div[class^="player-influence-"]').each(function() {
    UpdateAdjustBar(this, 0)
  });

  // Listen for updates from the server
  mixer.socket.on('event', data => {
    // Update the bars according to the new player state.
    $('div[class^="player-influence-"]').each(function() {
      const playerNumber = parseInt($(this).attr("class").split('-')[3]);
      UpdateAdjustBar(this, parseInt(data.player_values[playerNumber]));
    });
    let string = "";
    data.player_values.forEach(function(element) {
      string += ` ${element}`;
    });
    console.log(string);
  });

  // Setup the adjust buttons
  $('img[class^="adjust-button-"]').each(function() {
      $(this).hover(function(){
        if(adjustButtonsDisabled) return;
        const name = $(this).attr("class").split('-')[2];
        $(this).attr('src', `https://www.quinndamerell.com/dnd/${name}-hover.png`)
      },
      function() {
        if(adjustButtonsDisabled) return;
        const name = $(this).attr("class").split('-')[2];
        $(this).attr('src', `https://www.quinndamerell.com/dnd/${name}.png`)
      }
    )
    $(this).click(function()
    {
      const name = $(this).attr("class").split('-')[2];
      const number = $(this).attr("class").split('-')[3];
      OnPlayerAdjustButtonClick(parseInt(number), name === 'plus');
    });
  });

  // Setup the popups.
  $('.player-info').each(function() {
      $(this).hover(function(){
        $(this).find(".popup").fadeIn("slow");
      },
      function() {
        $(this).find(".popup").fadeOut("slow");
      }
    )
  });

  // Hide the overlay on mobile
  if (/Android|webOS|iPhone|iPad|iPod|BlackBerry|BB|PlayBook|IEMobile|Windows Phone|Kindle|Silk|Opera Mini/i.test(navigator.userAgent)) {
    $("#overlay").hide();
  }

  // Signal that we are loaded.
  mixer.isLoaded();
});

function UpdateAdjustBar(element, value)
{
  const name = $(element).attr("class").split('-')[2];
  const playerNumber = parseInt($(element).attr("class").split('-')[3]);
  const scaleElement = $(`.player-scale-${playerNumber}`)
  if(name === 'neg') {
     if(value > 0) value = 0;
  }
  else {
    if(value < 0) value = 0;
  }
  const width = Math.abs(value) / 2;
  const centerOfScale = scaleElement.offset().left + (scaleElement.width() / 2);
  const leftOffset = name === 'neg' ? centerOfScale - width : centerOfScale;
  $(element).offset({ top: scaleElement.offset().top - 5, left: leftOffset });
  $(element).width(width)
}

function OnPlayerAdjustButtonClick(playerNumber, isAdd)
{
  if(adjustButtonsDisabled)
  {
    return;
  }

  // Tell the server
  mixer.socket.call('giveInput', {
    controlID: 'tunnel-button',
    action: "player-influence",
    event: 'click',
    player: playerNumber,
    isAdd : isAdd
  });

  cooldownButtons();
}

function cooldownButtons() {
  adjustButtonsDisabled = true;
  // Disable the buttons
  $('img[class^="adjust-button-"]').each(function() {
    const name = $(this).attr("class").split('-')[2];
    $(this).attr('src', `https://www.quinndamerell.com/dnd/${name}-disabled.png`)
  })

  setTimeout(function()
  {
    adjustButtonsDisabled = false;
    $('img[class^="adjust-button-"]').each(function() {
      const name = $(this).attr("class").split('-')[2];
      $(this).attr('src', `https://www.quinndamerell.com/dnd/${name}.png`)
    })
  }, 1000);
}

function setupInitialPositions()
{
  setTimeout(setupInitialPositions, 1000);
  const pos = mixer.display.getPosition()
  if(pos == undefined)
  {
    return;
  }
  handleVideoResized(pos);
}

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
  footer.style.height = `${dndFooterHeight - 15}px`;
  footer.style.top = `${player.top + player.height}px`;
}
