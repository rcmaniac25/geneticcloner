import bb.cascades 1.0

Page {
    resizeBehavior: PageResizeBehavior.None
    
    Container {
        layout: StackLayout {
        }
        
        background: Color.DarkGray
        
        verticalAlignment: VerticalAlignment.Fill
        horizontalAlignment: HorizontalAlignment.Fill
        
        attachedObjects: [
            TextStyleDefinition {
                id: whiteFontStyle
                
                base: SystemDefaults.TextStyles.BodyText
                
                color: Color.White
            },
            TextStyleDefinition {
                id: clonerFontStyle
                
                base: whiteFontStyle.style
                
                fontSize: FontSize.Large
                color: Color.White
                fontWeight: FontWeight.Bold
            }
        ]
        
        Container {
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            
            horizontalAlignment: HorizontalAlignment.Center
            
            Container {
                layout: StackLayout {
                }
                
                leftPadding: 10.0
                topPadding: 10.0
                rightPadding: 10.0
                bottomPadding: 10.0
                
                Label {
                    text: "Source Image"
                    
                    textStyle {
                        base: clonerFontStyle.style
                    }
                }
                ImageView {
                    id: originalIV
                    objectName: "originalImageView"
                    
                    horizontalAlignment: HorizontalAlignment.Center
                    
                    scalingMethod: ScalingMethod.AspectFit
                    
                    preferredWidth: 256.0
                    preferredHeight: 256.0
                    
                    maxWidth: preferredWidth
                    maxHeight: preferredHeight
                }
            }
            
            Container {
                layout: StackLayout {
                }
                
                leftPadding: 10.0
                topPadding: 10.0
                rightPadding: 10.0
                bottomPadding: 10.0
                
                Label {
                    text: "Best Mutation"
                    
                    textStyle {
                        base: clonerFontStyle.style
                    }
                }
                ForeignWindowControl {
                    objectName: "mutatedFWC"
                    
                    horizontalAlignment: HorizontalAlignment.Center
                    
                    preferredWidth: originalIV.preferredWidth
                    preferredHeight: originalIV.preferredHeight
                }
            }
        }
        ImageView {
	        horizontalAlignment: HorizontalAlignment.Fill
	        
	        imageSource: "asset:///topGrad.png"
	        
	        preferredHeight: 18.0
	        
	        bottomMargin: 0.0
        }
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            
            background: Color.Black
            
	        Container {
	            layout: StackLayout {
	                orientation: LayoutOrientation.LeftToRight
	            }
	            
                horizontalAlignment: HorizontalAlignment.Center
                
                topPadding: 10.0
                bottomPadding: 10.0
                
                Label {
	                text: "Stats:"
	                
	                verticalAlignment: VerticalAlignment.Center
	                
	                textStyle {
	                    base: clonerFontStyle.style
	                    
	                    fontSize: FontSize.XLarge
                        fontWeight: FontWeight.W600
                    }
	            }
	            Container {
	                Label {
	                    text: cloner.fitness + "% fitness"
	                    
	                    textStyle {
                            base: whiteFontStyle.style
                        }
	                }
	                Label {
	                    text: cloner.improvements + " improvements"
	                    
	                    textStyle {
                            base: whiteFontStyle.style
                        }
	                }
	                Label {
	                    text: cloner.mutations + " mutations"
	                    
	                    textStyle {
                            base: whiteFontStyle.style
                        }
	                }
	            }
	            Button {
	                text: "Start mutation"
	                
	                verticalAlignment: VerticalAlignment.Center
	                
	                onClicked: {
	                    cloner.toggleRunning();
	                    if(text == "Start mutation")
	                    {
	                        text = "Stop";
	                    }
	                    else
	                    {
	                        text = "Start mutation";
	                    }
	                }
	            }
	        }
	    }
	    ImageView {
	        horizontalAlignment: HorizontalAlignment.Fill
	        
	        imageSource: "asset:///bottomGrad.png"
	        
	        preferredHeight: 19.0
	        
	        topMargin: 0.0
        }
    }
}

